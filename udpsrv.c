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

#define CMD_MSK    (0x3fffffff)
#define CMD_WR(a)  (0x40000000 | (((a)>>2)&CMD_MSK))
#define CMD_RD(a)  (0x00000000 | (((a)>>2)&CMD_MSK))

#define CMD_IS_RD(x) (((x)&0xc0000000) == 0x00000000)
#define CMD_IS_WR(x) (((x)&0xc0000000) == 0x40000000)
#define CMD_ADDR(x)  ( (x)<<2 )

#include <udpsrv_regdefs.h>
#include <udpsrv_port.h>

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


static void usage(const char *nm)
{
	fprintf(stderr, "usage: %s -a <inet_addr> -P <V2 port> [-s <stream_port>] [-f <n_frags>] [-L <loss_percent>] [-S <depth> ] [-V <protoVersion>]\n", nm);
	fprintf(stderr, "        -P          : port where V2 SRP server is listening (-1 for none)\n");
	fprintf(stderr, "        -p          : port where V1 SRP server is listening (-1 for none)\n");
#ifdef DEBUG
	fprintf(stderr, "        -d          : enable debugging messages (may slow down)\n");
#endif
	fprintf(stderr, "        -S <depth>  : scramble packet order (arg is ld2(scramble-length))\n");
	fprintf(stderr, "        -L <percent>: lose/drop <percent> packets\n");
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

typedef struct streamer_args {
	UdpPrt             port;
	unsigned           sim_loss;
	unsigned           n_frags;
	unsigned           fram;
	unsigned           scramble;
	unsigned           jam;
} streamer_args;

typedef struct srp_args {
	UdpPrt             port;
	int                v1;
	unsigned           sim_loss;
} srp_args;

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
uint8_t       buf[1500];
int           got;
int           lfram = -1;

	while ( (got = udpPrtRecv( sa->port, NULL, 0, buf, sizeof(buf))) >= 0 ) {
		if ( got < 4 ) {
			fprintf(stderr,"Poller: Contacted by %d\n", udpPrtIsConn( sa->port ) );
		} else {
			unsigned vers, fram, frag, tdest, tid, tusr1, tail;

			if ( got < 9 ) {
				sa->jam = 100; // frame not even large enough to be valid
				continue;
			}

			vers  = buf[0] & 0xf;
			fram  = (buf[1]<<8) | (buf[0]>>4);
			frag  = (((buf[4]<<8) | buf[3])<<8) | buf[2];
			tdest = buf[5];
			tid   = buf[6];
			tusr1 = buf[7];

			tail  = buf[got-1];

#ifdef DEBUG
			printf("Got stream message:\n");
			printf("   Version %d -- Frame #%d, frag #%d, tdest 0x%02x, tid 0x%02x, tusr1 0x%02x\n",
			       vers,
			       fram,
			       frag,
			       tdest,
			       tid,
			       tusr1);
#endif

			if ( vers != 0 || frag != 0 || tdest != 0 || tid != 0 || tusr1 != 0 ) {
				fprintf(stderr,"UDPSRV: Invalid stream header received\n");
				sa->jam = 100;
				continue;
			}

			// assume 1st frame of a new instance of the test program always starts at 0
			if ( fram > 0 ) {
				if ( lfram + 1 != fram ) {
					fprintf(stderr,"UDPSRV: Non-consecutive stream header received\n");
					sa->jam = 100;
					continue;
				}
			}
			lfram = fram;

			if ( tail != 0x80 ) {
				fprintf(stderr,"UDPSRV: Invalid stream tailbyte received\n");
				sa->jam = 100;
				continue;
			}
	
			/* This is for testing the stream-write feature. We don't actually send anything
			 * meaningful back.
			 * The test just verifies that the data we receive from the stream writer is
			 * correct (it's crc anyways).
			 * If this is not the case then we let the test program know by corrupting
			 * the stream data we send back on purpose.
			 */
			if ( crc32_le_t4(-1, buf + 8, got - 9) ^ -1 ^ CRC32_LE_POSTINVERT_GOOD ) {
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
		if ( udpPrtSend( sa->port, NULL, 0, bufmem[idx], sizeof(bufmem[idx])) < 0 ) {
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
int      i,j;
int      idx  = 0;
uint32_t crc  = -1;
int      end_of_frame;

	while (1) {
		if ( 0 == udpPrtIsConn( sa->port ) ) {
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

/* in bss; should thus be initialized before 
 * any of the range constructors are executed
 */
struct udpsrv_range *udpsrv_ranges = 0;

static void* srpHandler(void *arg)
{
srp_args *srp_arg = (srp_args*)arg;
uintptr_t rval = 1;
int      hsize, bsize;
int      st;
int      expected;
unsigned off;
int      got, put;
uint32_t addr = 0;
uint32_t xid  = 0;
uint32_t size = 16;
uint32_t rbuf[300];
uint32_t header = 0;
int      v1 = srp_arg->v1;
struct udpsrv_range *range;

UdpPrt port = srp_arg->port;

	expected = 12;

	if ( v1 ) {
		header = bs32( v1, 0 );
		hsize = sizeof(header);
		expected += 4;
	} else {
		hsize = 0;
	}

	while ( 1 ) {

		bsize = sizeof(rbuf);

		got = udpPrtRecv( port, &header, hsize, rbuf, bsize );
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

		st = -1;

		for ( range = udpsrv_ranges; range; range=range->next ) {
			if ( off >= range->base && off < range->base + range->size ) {
				if ( off + 4*size > range->base + range->size ) {
					fprintf(stderr,"%s request out of range (off 0x%x, size %d)\n", CMD_IS_RD(addr) ? "read" : "write", off, 4*size);
					fprintf(stderr,"range base 0x%x, size %d\n", range->base, range->size);
				} else {
					if ( CMD_IS_RD(addr) ) {
						st = range->read( &rbuf[2], size, off - range->base, debug );
						payload_swap( v1, &rbuf[2], size );
					} else {
						payload_swap( v1, &rbuf[2], (got-expected)/4 );
						st = range->write( &rbuf[2], size, off - range->base, debug );
					}
#ifdef DEBUG
					if (debug) {
						if ( CMD_IS_RD(addr) )
							printf("Read ");
						else
							printf("Wrote ");

						printf("(xid %x)\n", xid);

					}
#endif
				}
				break;
			}
		}

		
#ifdef DEBUG
		if ( debug && ! range ) {
			printf("No range matched 0x%08"PRIx32"\n", off);
		}
#endif
		if ( st ) {
			size=0;
		}
		memset( &rbuf[2+size], st ? 0xff : 0x00, 4);

		bs32(v1, rbuf[2+size] );

		bsize = (size + 3) * 4;

		if ( udpPrtSend( port, &header, hsize, rbuf, bsize ) < 0 ) {
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
	if ( port )
		udpPrtDestroy( port );
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
int     *i_a;
int      v2port   = V2PORT_DEF;
int      v1port   = V1PORT_DEF;
int      sport    = SPORT_DEF;
const char *ina   = INA_DEF;
int      n_frags  = NFRAGS;
int      sim_loss = SIMLOSS_DEF;
int      scramble = SCRMBL_DEF;

pthread_t poller_tid, fragger_tid, srp_tid;
int    have_poller  = 0;
int    have_fragger = 0;
int    have_srp     = 0;

struct streamer_args *s_arg   = 0;
struct srp_args      *srp_arg = 0;


	signal( SIGINT, sh );

	while ( (opt = getopt(argc, argv, "dP:p:a:hs:f:S:L:")) > 0 ) {
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

	if ( sport >= 0 ) {
		s_arg = malloc( sizeof(*s_arg) );
		if ( ! s_arg ) {
			fprintf(stderr,"No Memory\n");
			goto bail;
		}
		s_arg->port                 = udpPrtCreate( ina, sport, WITHOUT_RSSI );
		s_arg->sim_loss             = sim_loss;
		s_arg->n_frags              = n_frags;
		s_arg->scramble             = scramble;
		s_arg->fram                 = 0;
		s_arg->jam                  = 0;

		if ( ! s_arg->port )
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
		srp_arg->port     = udpPrtCreate( ina, v1port, WITHOUT_RSSI );
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
		arg.port     = udpPrtCreate( ina, ((arg.v1 = v2port < 0) ? v1port : v2port), WITHOUT_RSSI ); 
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
	if ( s_arg ) {
		if ( s_arg->port )
			udpPrtDestroy( s_arg->port );
		free( s_arg );
	}
	if ( srp_arg ) {
		udpPrtDestroy( srp_arg->port );
		free( srp_arg );
	}
	return rval;
}
