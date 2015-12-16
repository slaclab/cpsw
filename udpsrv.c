#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>

#define DEBUG

#define CMD_MSK    (0x3fffffff)
#define CMD_WR(a)  (0x40000000 | (((a)>>2)&CMD_MSK))
#define CMD_RD(a)  (0x00000000 | (((a)>>2)&CMD_MSK))

#define CMD_IS_RD(x) (((x)&0xc0000000) == 0x00000000)
#define CMD_IS_WR(x) (((x)&0xc0000000) == 0x40000000)
#define CMD_ADDR(x)  ( (x)<<2 )

#define REGBASE 0x1000 /* pseudo register space */
#define REG_RO_OFF 0
#define REG_RO_SZ 16
#define REG_CLR_OFF REG_RO_SZ
#define REG_SCR_OFF (REG_CLR_OFF + 8)
#define REG_SCR_SZ  32
#define REG_ARR_OFF (REG_SCR_OFF + REG_SCR_SZ)
#define REG_ARR_SZ  8192

#define NFRAGS  4
#define FRAGLEN 8

#define FRAMBITS 12
#define FRAGBITS 24
#define VERS     0
#define VERSBITS 4

#define HEADSIZE 8
#define TAILSIZE 1

#define EOFRAG   0x80

#define PIPEDEPTH (1<<0) /* MUST be power of two */

// byte swap ? 
#define bsl(x) (x)

#define BE 1
#define LE 0

typedef uint8_t Buf[FRAGLEN + HEADSIZE + TAILSIZE];

static Buf bufmem[PIPEDEPTH];

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
	fprintf(stderr, "usage: %s -a <inet_addr> -p <port> [-s <stream_port>] [-f <n_frags>] [-L <loss_percent>] [-S] [-V <protoVersion>]\n", nm);
	fprintf(stderr, "        -V version  : protocol version V1 (V2 default)\n");
#ifdef DEBUG
	fprintf(stderr, "        -d          : disable debugging messages (faster)\n");
	fprintf(stderr, "        -S          : scramble packet order (arg is ld2(scramble-length)) \n");
	fprintf(stderr, "        -L <percent>: lose/drop <percent> pacets\n");
#endif
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
	me.sin_addr.s_addr = inet_addr(ina);
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

struct streamer_args {
	int                sd;
	struct sockaddr_in peer;
	unsigned           sim_loss;
	unsigned           n_frags;
	unsigned           scramble;
};

void *poller(void *arg)
{
struct streamer_args *sa = (struct streamer_args*)arg;
char      buf[8];
socklen_t l;

	while ( (l=sizeof(sa->peer), recvfrom(sa->sd, buf, sizeof(buf), 0, (struct sockaddr*)&sa->peer, &l)) >= 0 ) {
		fprintf(stderr,"Poller: Contacted by %d\n", ntohs(sa->peer.sin_port));
	}
	perror("Poller thread failed");
	exit(1);
	return 0;
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
	if ( !sa->sim_loss || rand() > (RAND_MAX/100)*sa->sim_loss ) {
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
unsigned fram = 0;
uint64_t h;
struct iovec iov[2];
int      i;
int      idx  = 0;

	while (1) {
		if ( 0 == ntohs( sa->peer.sin_port ) ) {
			sleep(4);
			continue;
		}
		h = mkhdr(fram, frag);
		for ( i=0; i<HEADSIZE; i++ ) {
			bufmem[idx][i] = h;
			h           = h>>8; 
		}
		memset(bufmem[idx] + i, (fram << 4) | (frag & 0xf), FRAGLEN);
		i += FRAGLEN;
		bufmem[idx][i] = (sa->n_frags-1 == frag) ? EOFRAG : 0;
		i++;

		idx = send_shuffled(idx, sa);

		if ( ++frag >= sa->n_frags ) {
			frag = 0;
			fram++;
			sleep(1);
		}
	}
	fprintf(stderr,"Fragmenter thread failed\n");
	exit(1);

	return 0;
}

int
main(int argc, char **argv)
{
int sd = -1;
int rval = 1;
struct   sockaddr_in you;
uint32_t rbuf[2048];
int      n, got, put;
uint32_t addr = 0;
uint32_t size = 16;
char *c_a;
int  *i_a;
int        port = 8192;
int       sport = 0;
const char *ina = "127.0.0.1";
socklen_t  youlen;
unsigned off;
unsigned n_frags  = NFRAGS;
unsigned sim_loss = 0;
unsigned scramble = 0;
int      vers = 2;
int      v1;
uint32_t header = 0;

pthread_t poller_tid, fragger_tid;
int    have_poller  = 0;
int    have_fragger = 0;

struct msghdr mh;
struct iovec  iov[2];
int    niov = 0;
int    i;
int    expected;
int    debug = 
#ifdef DEBUG
	1
#else
	0
#endif
;

struct streamer_args *s_arg = 0;

	memset( &mh, 0, sizeof(mh) );

	sprintf(mem + 0x800, "Hello");

	while ( (got = getopt(argc, argv, "dp:a:V:hs:f:S:L:")) > 0 ) {
		c_a = 0;
		i_a = 0;
		switch ( got ) {
			case 'h': usage(argv[0]); return 0;
			case 'p': i_a = &port;     break;
			case 'a': ina = optarg;    break;
			case 'V': i_a = &vers;     break;
			case 'd': debug = 0;       break;
			case 's': i_a = &sport;    break;
			case 'f': i_a = &n_frags;  break;
			case 'L': i_a = &sim_loss; break;
			case 'S': i_a = &scramble; break;
			default:
				fprintf(stderr, "unknown option '%c'\n", got);
				usage(argv[0]);
				goto bail;
		}
		if ( i_a ) {
			if ( 1 != sscanf(optarg,"%i",i_a) ) {
				fprintf(stderr,"Unable to scan option '-%c' argument '%s'\n", got, optarg);
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

	if ( vers != 1 && vers != 2 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1 or 2\n", vers);
		goto bail;
	}

	if ( sport && sport == port ) {
		fprintf(stderr,"Stream and SRP ports must be different!\n");
		goto bail;
	}

	if ( ! ina || ! port ) {
		fprintf(stderr,"Need inet and port args (-a and -p options)\n");
		goto bail;
	}

	v1 = (vers == 1);

	if ( (sd = getsock( ina, port ) ) < 0 )
		goto bail;

	if ( sport ) {
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

	expected = 12;

	if ( v1 ) {
		header = bs32( v1, 0 );
		iov[niov].iov_base = &header;
		iov[niov].iov_len  = sizeof(header);
		expected += 4;
		niov++;
	}

	for ( i=0; i<16; i+=2 ) {
		mem[REGBASE+i/2]    = (i<<4)|(i+1);
		mem[REGBASE+15-i/2] = (i<<4)|(i+1);
	}

	while ( 1 ) {

		youlen = sizeof(you);

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

		addr = bs32(v1,  rbuf[1] );

		off = CMD_ADDR(addr);

		if ( CMD_IS_RD(addr) ) {
			size = bs32(v1,  rbuf[2] ) + 1;
		} else {
			size = (got - expected)/4;
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

			printf(" %x (%d); %d words\n", off, off, size);

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
	if ( have_poller ) {
		void *ign;
		pthread_cancel( poller_tid );
		pthread_join( poller_tid, &ign );
	}
	if ( have_fragger ) {
		void *ign;
		pthread_cancel( fragger_tid );
		pthread_join( fragger_tid, &ign );
	}
	if ( s_arg )
		free( s_arg );
	return rval;
}
