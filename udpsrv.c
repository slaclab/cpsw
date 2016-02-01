#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <crc32-le-tbl-4.h>

#define DEBUG

#define CMD_MSK    (0x3fffffff)
#define CMD_WR(a)  (0x40000000 | (((a)>>2)&CMD_MSK))
#define CMD_RD(a)  (0x00000000 | (((a)>>2)&CMD_MSK))

#define CMD_IS_RD(x) (((x)&0xc0000000) == 0x00000000)
#define CMD_IS_WR(x) (((x)&0xc0000000) == 0x40000000)
#define CMD_ADDR(x)  ( (x)<<2 )

#include <udpsrv_regdefs.h>

#define NFRAGS  20
#define FRAGLEN 8

#define FRAMBITS 12
#define FRAGBITS 24
#define VERS     0
#define VERSBITS 4

#define HEADSIZE 8
#define TAILSIZE 1

#define EOFRAG   0x80
#define PIPEDEPTH (1<<4) /* MUST be power of two */

#define V1PORT_DEF 8191
#define V2PORT_DEF 8192
#define SPORT_DEF  8193
#define SCRMBL_DEF 2
#define INA_DEF    "127.0.0.1"
#define SIMLOSS_DEF 0
#define NFRAGS_DEF  NFRAGS

// byte swap ? 
#define bsl(x) (x)

#define BE 1
#define LE 0

typedef uint8_t Buf[FRAGLEN + HEADSIZE + TAILSIZE];

static Buf bufmem[PIPEDEPTH];
static int debug = 0;

static inline uint32_t swp32(int to, uint32_t x)
{
union { uint16_t s; uint8_t c[2]; } u = { .s = 1 };
	if ( u.c[to] )
		return x;
	else
		return __builtin_bswap32(x);
}

// protocol wants LE
#define bs32(v1, x) swp32(v1 ? BE : LE, x)


uint8_t mem[1024*1024] = {0};

static void usage(const char *nm)
{
	fprintf(stderr, "usage: %s -a <inet_addr> -P <V2 port> [-s <stream_port>] [-f <n_frags>] [-L <loss_percent>] [-S <depth> ] [-V <protoVersion>]\n", nm);
	fprintf(stderr, "        -P          : port where V2 SRP server is listening (-1 for none)\n");
	fprintf(stderr, "        -p          : port where V1 SRP server is listening (-1 for none)\n");
#ifdef DEBUG
	fprintf(stderr, "        -d          : enable debugging messages (may slow down)\n");
#endif
	fprintf(stderr, "        -S <depth>  : scramble packet order (arg is ld2(scramble-length))\n");
	fprintf(stderr, "        -L <percent>: lose/drop <percent> pacets\n");
	fprintf(stderr, " Defaults: -a %s -P %d -p %d -s %d -f %d -L %d -S %d\n", INA_DEF, V2PORT_DEF, V1PORT_DEF, SPORT_DEF, NFRAGS_DEF, SIMLOSS_DEF, SCRMBL_DEF);
}

static void payload_swap(int v1, uint32_t *buf, int nelms)
{
int i;
	if ( ! v1 )
		return;
	for ( i=0; i<nelms; i++ )
		buf[i] = __builtin_bswap32( buf[i] );
}

static int getsock(const char *ina, unsigned short port)
{
int sd = -1;
struct sockaddr_in me;

	if ( (sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket");
		goto bail;
	}

	me.sin_family      = AF_INET;
	me.sin_addr.s_addr = ina ? inet_addr(ina) : INADDR_ANY;
	me.sin_port        = htons( (short)port );

	if ( bind( sd, (struct sockaddr*)&me, sizeof(me) ) < 0 ) {
		perror("bind");
		goto bail;
	}

	return sd;

bail:
	close(sd);
	return -1;
}

typedef struct streamer_args {
	int                sd;
	struct sockaddr_in peer;
	unsigned           sim_loss;
	unsigned           n_frags;
	unsigned           fram;
	unsigned           scramble;
	pthread_mutex_t    mtx;
	unsigned           jam;
} streamer_args;

typedef struct srp_args {
	const char *       ina;
	int                port;
	int                v1;
	unsigned           sim_loss;
} srp_args;

static void mustLock(pthread_mutex_t *m)
{
	if ( pthread_mutex_lock( m ) ) {
		perror("FATAL ERROR -- unable to lock mutex!");
		exit(1);
	}
}

static void mustUnlock(pthread_mutex_t *m)
{
	if ( pthread_mutex_unlock( m ) ) {
		perror("FATAL ERROR -- unable to unlock mutex!");
		exit(1);
	}
}

static uint64_t mkhdr(unsigned fram, unsigned frag)
{
uint64_t rval;
	if ( HEADSIZE > sizeof(rval) ) {
		fprintf(stderr,"FATAL ERROR -- HEADSIZE too big\n");
		exit(1);
	}
	rval = (frag & ((1<<FRAGBITS)-1));
	rval = rval << FRAMBITS;
	rval |= (fram & ((1<<FRAMBITS)-1));
	rval = rval << VERSBITS;
	rval |= VERS & ((1<<VERSBITS)-1);
	return rval;
}

static int insert_header(uint8_t *hbuf, unsigned fram, unsigned frag)
{
uint64_t h;
int      i;
	h = mkhdr(fram, frag);
	for ( i=0; i<HEADSIZE; i++ ) {
		hbuf[i] = h;
		h       = h>>8; 
	}
	return i;
}

static int append_tail(uint8_t *tbuf, int eof)
{
	*tbuf = eof ? EOFRAG : 0;
	return 1;
}


void *poller(void *arg)
{
struct streamer_args *sa = (struct streamer_args*)arg;
char          buf[2048];
int           got;
socklen_t     l;
struct iovec  iov[3];
struct msghdr mh;

uint8_t       hbuf[HEADSIZE];
uint8_t       tbuf;

	mh.msg_name       = (struct sockaddr*)&sa->peer;
	mh.msg_namelen    = sizeof( sa->peer ); 
	mh.msg_iov        = iov;
	mh.msg_iovlen     = sizeof(iov)/sizeof(iov[0]);
	mh.msg_control    = 0;
	mh.msg_controllen = 0;
	mh.msg_flags      = 0;

	while ( (l=sizeof(sa->peer), got = recvfrom(sa->sd, buf, sizeof(buf), 0, (struct sockaddr*)&sa->peer, &l)) >= 0 ) {
		if ( got <= 8 ) {
			fprintf(stderr,"Poller: Contacted by %d\n", ntohs(sa->peer.sin_port));
		} else {
			/* This is for testing the stream-write feature. We don't actually send anything
			 * meaningful back.
			 * The test just verifies that the data we receive from the stream writer is
			 * correct (it's crc anyways).
			 * If this is not the case then we let the test program know by corrupting
			 * the stream data we send back on purpose.
			 */
			if ( crc32_le_t4(-1, buf, got) ^ -1 ^ CRC32_LE_POSTINVERT_GOOD ) {
				fprintf(stderr,"Received message (size %d) with bad CRC\n", got);
				sa->jam = 100;
printf("JAM\n");
			}
		}
	}
	perror("Poller thread failed");
	exit(1);
	return 0;
}

static int send_shuffled(int idx, struct streamer_args *sa)
{
static int depth = 0;
	if ( depth < sa->scramble - 1 ) {
		if (RAND_MAX < (PIPEDEPTH << 16) ) {
			fprintf(stderr,"FAILURE: PIPEDEPTH problem\n");
			exit(1);
		}
		return ++depth;
	}
	idx = (rand() >> 16) & (sa->scramble-1);

	/* simulate packet loss */
	if ( !sa->sim_loss || ( (double)rand() > ((double)RAND_MAX/100.)*(double)sa->sim_loss/(double)sa->n_frags ) ) {
		if ( sendto(sa->sd, bufmem[idx], sizeof(bufmem[idx]), 0, (struct sockaddr*)&sa->peer, sizeof(sa->peer)) < 0 ) {
			perror("fragmenter: write");
			switch ( errno ) {
				case EDESTADDRREQ:
				case ECONNREFUSED:
					/* not (yet/anymore) connected -- wait */
					sleep(10);
					return idx;
				default:
					return -1;
			}
			return -1;
		}
	}
	return idx;
}

void *fragger(void *arg)
{
struct streamer_args *sa = (struct streamer_args*)arg;
unsigned frag = 0;
struct iovec iov[2];
int      i,j;
int      idx  = 0;
uint32_t crc  = -1;
int      end_of_frame;

	while (1) {
		if ( 0 == ntohs( sa->peer.sin_port ) ) {
			sleep(4);
			continue;
		}

		i  = 0;
		i += insert_header(bufmem[idx], sa->fram, frag);

		memset(bufmem[idx] + i, (sa->fram << 4) | (frag & 0xf), FRAGLEN);

		end_of_frame = (sa->n_frags - 1 == frag);

		crc = crc32_le_t4(crc, bufmem[idx] + i, end_of_frame ? FRAGLEN - sizeof(crc) : FRAGLEN);

		if ( end_of_frame ) {
			crc ^= -1;
			if ( sa->jam ) {
				crc ^= 0xdeadbeef;
				sa->jam--; // assume atomic access...
if ( ! sa->jam )
printf("JAM cleared\n");
			}
			for ( j =0; j<sizeof(crc); j++ ) {
				* (bufmem[idx] + i + FRAGLEN - sizeof(crc) + j) = crc & 0xff;
				crc >>= 8;
			}
		}

		i += FRAGLEN;

		i += append_tail( &bufmem[idx][i], end_of_frame );

		idx = send_shuffled(idx, sa);

		if ( ++frag >= sa->n_frags ) {
			frag = 0;
			crc  = -1;
			sa->fram++;
			struct timespec ts;
			ts.tv_sec  = 0;
			ts.tv_nsec = 10000000;
			nanosleep( &ts, 0 );
		}
	}
	fprintf(stderr,"Fragmenter thread failed\n");
	exit(1);

	return 0;
}

static void* srpHandler(void *arg)
{
srp_args *srp_arg = (srp_args*)arg;
uintptr_t rval = 1;
int      sd;
struct msghdr mh;
struct iovec  iov[2];
int      niov = 0;
int      i;
int      expected;
unsigned off;
int      n, got, put;
uint32_t addr = 0;
uint32_t xid  = 0;
uint32_t size = 16;
uint32_t rbuf[2048];
uint32_t header = 0;
struct   sockaddr_in you;
int      v1 = srp_arg->v1;

	if ( (sd = getsock( srp_arg->ina, srp_arg->port ) ) < 0 )
		goto bail;

	expected = 12;

	if ( v1 ) {
		header = bs32( v1, 0 );
		iov[niov].iov_base = &header;
		iov[niov].iov_len  = sizeof(header);
		expected += 4;
		niov++;
	}

	while ( 1 ) {

		memset( &mh, 0, sizeof(mh) );
		mh.msg_name    = (struct sockaddr*)&you;
		mh.msg_namelen = sizeof(you);

		iov[niov].iov_base = rbuf;
		iov[niov].iov_len  = sizeof(rbuf);

		mh.msg_iov    = iov;
		mh.msg_iovlen = niov + 1;

		got = recvmsg(sd, &mh, 0);
		if ( got < 0 ) {
			perror("read");
			goto bail;
		}

		xid  = bs32(v1,  rbuf[0] );
		addr = bs32(v1,  rbuf[1] );

		off = CMD_ADDR(addr);

		if ( CMD_IS_RD(addr) ) {
			size = bs32(v1,  rbuf[2] ) + 1;
			if ( got != expected + 4 /* status word */ ) {
printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"READ command -- got != expected + 4; dropping...\n");
				continue;
			}
		} else {
			if ( got < expected ) {
printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"WRITE command -- got < expected; dropping...\n");
				continue;
			} else if ( (got % 4) != 0 ) {
printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"WRITE command -- got not a multiple of 4; dropping...\n");
				continue;
			}
			size = (got - expected)/4;
		}

		if ( rand() < (RAND_MAX/100)*srp_arg->sim_loss ) {
			/* simulated packet loss */
			continue;
		}

		if ( off + 4*size > sizeof(mem) ) {
			fprintf(stderr,"%s request out of range (off %d, size %d)\n", CMD_IS_RD(addr) ? "read" : "write", off, size);
			size=0;
			memset( &rbuf[2+size], 0xff, 4);
		} else { 
			if ( CMD_IS_RD(addr) ) {
				memcpy((void*) &rbuf[2], mem+off, size*4);
				payload_swap( v1, &rbuf[2], size );
			} else {
				payload_swap( v1, &rbuf[2], (got-expected)/4 );
				if ( off <= REGBASE + REG_RO_OFF && off + 4*size >= REGBASE + REG_RO_OFF + REG_RO_SZ ) {
					/* disallow write; set status */
					memset(&rbuf[2+size], 0xff, 4);
#ifdef DEBUG
if ( debug )
	printf("Rejecting write to read-only region\n");
#endif
				} else {
					memcpy(mem + off, (void*)&rbuf[2], got-expected);
					if ( off <= REGBASE + REG_CLR_OFF && off + 4*size >= REGBASE + REG_CLR_OFF + 4 ) {
						if (    mem[REGBASE + REG_CLR_OFF]
						     || mem[REGBASE + REG_CLR_OFF + 1]
						     || mem[REGBASE + REG_CLR_OFF + 2]
						     || mem[REGBASE + REG_CLR_OFF + 3]
						   ) {
							memset( mem + REGBASE + REG_SCR_OFF, 0xff, REG_SCR_SZ );
							/* write array in LE; first and last word have special pattern */
							mem[REGBASE + REG_ARR_OFF + 0] = 0xaa;
							mem[REGBASE + REG_ARR_OFF + 1] = 0xbb;
							mem[REGBASE + REG_ARR_OFF + 2] = 0xcc;
							mem[REGBASE + REG_ARR_OFF + 3] = 0xdd;
							for ( i=4; i<REG_ARR_SZ-4; i+=2 ) {
								mem[REGBASE + REG_ARR_OFF + i + 0] = i>>(0+1); // i.e., i/2
								mem[REGBASE + REG_ARR_OFF + i + 1] = i>>(8+1);
							}
							mem[REGBASE + REG_ARR_OFF + i + 0] = 0xfa;
							mem[REGBASE + REG_ARR_OFF + i + 1] = 0xfb;
							mem[REGBASE + REG_ARR_OFF + i + 2] = 0xfc;
							mem[REGBASE + REG_ARR_OFF + i + 3] = 0xfd;
#ifdef DEBUG
if ( debug )
	printf("Setting\n");
#endif
						} else {
							memset( mem + REGBASE + REG_SCR_OFF, 0x00, REG_SCR_SZ );
							/* write array in BE; first and last word have special pattern */
							mem[REGBASE + REG_ARR_OFF + 0] = 0xdd;
							mem[REGBASE + REG_ARR_OFF + 1] = 0xcc;
							mem[REGBASE + REG_ARR_OFF + 2] = 0xbb;
							mem[REGBASE + REG_ARR_OFF + 3] = 0xaa;
							for ( i=4; i<REG_ARR_SZ-4; i+=2 ) {
								mem[REGBASE + REG_ARR_OFF + i + 1] = i>>(0+1); // i.e., i/2
								mem[REGBASE + REG_ARR_OFF + i + 0] = i>>(8+1);
							}
							mem[REGBASE + REG_ARR_OFF + i + 0] = 0xfd;
							mem[REGBASE + REG_ARR_OFF + i + 1] = 0xfc;
							mem[REGBASE + REG_ARR_OFF + i + 2] = 0xfb;
							mem[REGBASE + REG_ARR_OFF + i + 3] = 0xfa;
#ifdef DEBUG
if ( debug )
	printf("Clearing\n");
#endif
						}
						memset( mem + REGBASE + REG_CLR_OFF, 0xaa, 4 );
					}
				}
			}
			memset((void*) &rbuf[2+size], 0, 4);

#ifdef DEBUG
if (debug) {
			if ( CMD_IS_RD(addr) )
				printf("Read from");
			else
				printf("Writing to");

			printf(" %x (%d); %d words (xid %x)\n", off, off, size, xid);

			for ( n=0; n<size*4; n++ ) {
				printf("%02x ",mem[off+n]);	
				if ( (n & 15) == 15 ) printf("\n");
			}
			if ( n & 15 )
				printf("\n");
}
#endif
		}
		bs32(v1, rbuf[2+size] );

		iov[niov].iov_len = (size+3)*4;

		if ( (put = sendmsg( sd, &mh, 0 )) < 0 ) {
			perror("unable to send");
			goto bail;
		}
#ifdef DEBUG
if ( debug )
	printf("put %i\n", put);
#endif

	}

	rval = 0;
bail:
	if ( sd >= 0 )
		close( sd );
	return (void*)rval;
}

static void sh(int sn)
{
	fflush(stdout);
	fflush(stderr);
	exit(0);
}

int
main(int argc, char **argv)
{
int      rval = 1;
int      opt;
char    *c_a;
int     *i_a;
int      i;
int      v2port   = V2PORT_DEF;
int      v1port   = V1PORT_DEF;
int      sport    = SPORT_DEF;
const char *ina   = INA_DEF;
unsigned n_frags  = NFRAGS;
unsigned sim_loss = SIMLOSS_DEF;
unsigned scramble = SCRMBL_DEF;

pthread_t poller_tid, fragger_tid, srp_tid;
int    have_poller  = 0;
int    have_fragger = 0;
int    have_srp     = 0;

struct streamer_args *s_arg   = 0;
struct srp_args      *srp_arg = 0;

	sprintf(mem + 0x800, "Hello");

	signal( SIGINT, sh );

	while ( (opt = getopt(argc, argv, "dP:p:a:hs:f:S:L:")) > 0 ) {
		c_a = 0;
		i_a = 0;
		switch ( opt ) {
			case 'h': usage(argv[0]); return 0;
			case 'P': i_a = &v2port;   break;
			case 'p': i_a = &v1port;   break;
			case 'a': ina = optarg;    break;
			case 'd': debug = 1;       break;
			case 's': i_a = &sport;    break;
			case 'f': i_a = &n_frags;  break;
			case 'L': i_a = &sim_loss; break;
			case 'S': i_a = &scramble; break;
			default:
				fprintf(stderr, "unknown option '%c'\n", opt);
				usage(argv[0]);
				goto bail;
		}
		if ( i_a ) {
			if ( 1 != sscanf(optarg,"%i",i_a) ) {
				fprintf(stderr,"Unable to scan option '-%c' argument '%s'\n", opt, optarg);
				goto bail;
			}
		}
	}

	if ( sim_loss >= 100 ) {
		fprintf(stderr,"-L arg must be < 100\n");
		exit(1);
	}

	// arg is ld2
	scramble = 1<<scramble;
	if ( scramble > PIPEDEPTH ) {
		fprintf(stderr,"-S arg must be <= log2(%d)\n", PIPEDEPTH);
		exit(1);
	}

	if ( sport <= 0 && v1port <= 0 && v2port <= 0 ) {
		fprintf(stderr,"At least one of -s, -p, -P must be > 0\n");
		goto bail;
	}

	for ( i=0; i<16; i+=2 ) {
		mem[REGBASE+i/2]    = (i<<4)|(i+1);
		mem[REGBASE+15-i/2] = (i<<4)|(i+1);
	}

	if ( sport >= 0 ) {
		s_arg = malloc( sizeof(*s_arg) );
		if ( ! s_arg ) {
			fprintf(stderr,"No Memory\n");
			goto bail;
		}
		s_arg->sd = getsock( ina, sport );
		s_arg->peer.sin_family      = AF_INET;
		s_arg->peer.sin_addr.s_addr = INADDR_ANY;
		s_arg->peer.sin_port        = htons(0);
		s_arg->sim_loss             = sim_loss;
		s_arg->n_frags              = n_frags;
		s_arg->scramble             = scramble;
		s_arg->fram                 = 0;
		s_arg->jam                  = 0;
		if ( pthread_mutex_init( &s_arg->mtx, NULL ) ) {
			perror("pthread_mutex_init failed:");
			goto bail;
		}

		if ( s_arg->sd < 0 )
			goto bail;
		if ( pthread_create( &poller_tid, 0, poller, (void*)s_arg ) ) {
			perror("pthread_create [poller]");
			goto bail;
		}
		have_poller = 1;
		if ( pthread_create( &fragger_tid, 0, fragger, (void*)s_arg ) ) {
			perror("pthread_create [fragger]");
			goto bail;
		}
		have_fragger = 1;
	}

	if ( v2port >= 0 && v1port >= 0 ) {
		srp_arg = malloc(sizeof(*srp_arg));
		if ( ! srp_arg ) {
			fprintf(stderr,"No Memory\n");
			goto bail;
		}
		srp_arg->ina      = ina;
		srp_arg->port     = v1port;
		srp_arg->v1       = 1;
		srp_arg->sim_loss = sim_loss;

		if ( pthread_create( &srp_tid, 0, srpHandler, (void*)srp_arg ) ) {
			perror("pthread_create [srpHandler]");
			goto bail;
		}
		have_srp = 1;
	}

	if ( v1port >= 0 || v2port >= 0 ) {
		srp_args arg;
		arg.ina      = ina;
		arg.port     = (arg.v1 = v2port < 0) ? v1port : v2port; 
		arg.sim_loss = sim_loss;
	
		rval = (uintptr_t)srpHandler( &arg );
	}

bail:
	if ( have_poller ) {
		void *res;
		pthread_cancel( poller_tid );
		pthread_join( poller_tid, &res );
		rval += (uintptr_t)res;
	}
	if ( have_fragger ) {
		void *res;
		pthread_cancel( fragger_tid );
		pthread_join( fragger_tid, &res );
		rval += (uintptr_t)res;
	}
	if ( have_srp ) {
		void *res;
		pthread_cancel( srp_tid );
		pthread_join( srp_tid, &res );
		rval += (uintptr_t)res;
	}
	if ( s_arg )
		free( s_arg );
	if ( srp_arg )
		free( srp_arg );
	return rval;
}
