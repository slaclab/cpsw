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

#define TUSR1BITS  8
#define TIDBITS    8
#define TDESTBITS  8
#define FRAGBITS  24
#define FRAMBITS  12
#define VERS       0
#define VERSBITS   4

#define HEADSIZE 8
#define TAILSIZE 1

#define EOFRAG   0x80

#define V1PORT_DEF     8191
#define V2PORT_DEF     8192
#define V2RSSIPORT_DEF 8202
#define SPORT_DEF      8193
#define SRSSIPORT_DEF  8203
#define SCRMBL_DEF     1
#define INA_DEF        "127.0.0.1"
#define SIMLOSS_DEF     1
#define NFRAGS_DEF      NFRAGS

// byte swap ?
#define bsl(x) (x)

#define BE 1
#define LE 0

typedef uint8_t Buf[FRAGLEN + HEADSIZE + TAILSIZE];

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
	fprintf(stderr, "usage: %s -a <inet_addr> -P <V2 port> [-s <stream_port>] [-f <n_frags>] [-L <loss_percent>] [-S <lddepth> ] [-V <protoVersion>] [-R]\n", nm);
	fprintf(stderr, "        -P          : port where V2 SRP server is listening (-1 for none)\n");
	fprintf(stderr, "        -p          : port where V1 SRP server is listening (-1 for none)\n");
	fprintf(stderr, "        -s          : port where STREAM server is listening (-1 for none)\n");
	fprintf(stderr, "        -r          : port where V2 SRP server is listening (-1 for none) with RSSI\n");
	fprintf(stderr, "        -R          : port where STREAM server is listening (-1 for none) with RSSI\n");
#ifdef DEBUG
	fprintf(stderr, "        -d          : enable debugging messages (may slow down)\n");
#endif
	fprintf(stderr, "        -S <lddepth>: scramble packet order (arg is ld2(scrambler-depth))\n");
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
	unsigned           n_frags;
	unsigned           fram;
	unsigned           tdest;
	unsigned           jam;
	pthread_t          poller_tid, fragger_tid;
	int                haveThreads;
} streamer_args;

typedef struct srp_args {
	UdpPrt             port;
	int                vers;
	pthread_t          tid;
	int                haveThread;
} srp_args;

static uint64_t mkhdr(unsigned fram, unsigned frag, unsigned tdest)
{
uint64_t rval;
uint8_t  tid   = 0;
uint8_t  tusr1 = 0;
	if ( HEADSIZE > sizeof(rval) ) {
		fprintf(stderr,"FATAL ERROR -- HEADSIZE too big\n");
		exit(1);
	}
	rval   = 0;

	if ( frag == 0 )
		tusr1 |= (1<<1); // SOF

	rval <<= TUSR1BITS;
	rval  |= (tusr1 & ((1<<TUSR1BITS)-1));

	rval <<= TIDBITS;
	rval  |= (tid   & ((1<<TIDBITS)-1));

	rval <<= TDESTBITS;
	rval  |= (tdest & ((1<<TDESTBITS)-1));

	rval <<= FRAGBITS;
	rval  |= (frag & ((1<<FRAGBITS)-1));

	rval <<= FRAMBITS;
	rval  |= (fram & ((1<<FRAMBITS)-1));

	rval <<= VERSBITS;
	rval  |= VERS & ((1<<VERSBITS)-1);
	return rval;
}

static int insert_header(uint8_t *hbuf, unsigned fram, unsigned frag, unsigned tdest)
{
uint64_t h;
int      i;
	h = mkhdr(fram, frag, tdest);
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
			fram  = (buf[1]<<4) | (buf[0]>>4);
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

			if ( vers != 0 || frag != 0 || tdest != 0 || tid != 0 || tusr1 != (frag == 0 ? (1<<1) : 0) ) {
				fprintf(stderr,"UDPSRV: Invalid stream header received\n");
				sa->jam = 100;
				continue;
			}

			// assume 1st frame of a new instance of the test program always starts at 0
			if ( fram > 0 ) {
				if ( lfram + 1 != fram ) {
					fprintf(stderr,"UDPSRV: Non-consecutive stream header received\n");
					sa->jam = 100;
					lfram = fram;
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

void *fragger(void *arg)
{
struct streamer_args *sa = (struct streamer_args*)arg;
unsigned frag = 0;
int      i,j;
uint32_t crc  = -1;
int      end_of_frame;
Buf      bufmem;

	while (1) {
		if ( 0 == udpPrtIsConn( sa->port ) ) {
			sleep(4);
			continue;
		}

		i  = 0;
		i += insert_header(bufmem, sa->fram, frag, sa->tdest);

		memset(bufmem + i, (sa->fram << 4) | (frag & 0xf), FRAGLEN);

		end_of_frame = (sa->n_frags - 1 == frag);

		crc = crc32_le_t4(crc, bufmem + i, end_of_frame ? FRAGLEN - sizeof(crc) : FRAGLEN);

		if ( end_of_frame ) {
			crc ^= -1;
			if ( sa->jam ) {
				crc ^= 0xdeadbeef;
				sa->jam--; // assume atomic access...
if ( ! sa->jam )
printf("JAM cleared\n");
			}
			for ( j =0; j<sizeof(crc); j++ ) {
				* (bufmem + i + FRAGLEN - sizeof(crc) + j) = crc & 0xff;
				crc >>= 8;
			}
		}

		i += FRAGLEN;

		i += append_tail( &bufmem[i], end_of_frame );

		if ( udpPrtSend( sa->port, NULL, 0, bufmem, i ) < 0 ) {
			perror("fragmenter: write");
			switch ( errno ) {
				case EDESTADDRREQ:
				case ECONNREFUSED:
					/* not (yet/anymore) connected -- wait */
					sleep(10);
					continue;
				default:
					break;
			}
			break;
		}

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
uint64_t off;
int      got, put;
uint32_t addr = 0;
uint32_t xid  = 0;
uint32_t size = 16;
uint32_t rbuf[300];
uint32_t header = 0;
int      v1 = (srp_arg->vers == 1);
int      v3 = (srp_arg->vers == 3);
struct udpsrv_range *range;
int      cmd_rd;

UdpPrt port = srp_arg->port;

	expected = v3 ? 20 : 12;

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

		xid    = bs32(v1,  rbuf[0] );
		addr   = bs32(v1,  rbuf[1] );

		off    = CMD_ADDR(addr);
		cmd_rd = CMD_IS_RD(addr);

		if ( cmd_rd ) {
			size = bs32(v1,  rbuf[2] ) + 1;
			if ( got != expected + 4 /* status word */ ) {
				int j;
printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"READ command -- got != expected + 4; dropping...\n");
				for ( j = 0; j<(got+3)/4; j++ )
					fprintf(stderr,"  0x%08x\n", rbuf[j]);

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

		st = -1;

		for ( range = udpsrv_ranges; range; range=range->next ) {
			if ( off >= range->base && off < range->base + range->size ) {
				if ( off + 4*size > range->base + range->size ) {
					fprintf(stderr,"%s request out of range (off 0x%"PRIx64", size %d)\n", cmd_rd ? "read" : "write", off, 4*size);
					fprintf(stderr,"range base 0x%"PRIx64", size %"PRId64"\n", range->base, range->size);
				} else {
					if ( cmd_rd ) {
						st = range->read( &rbuf[2], size, off - range->base, debug );
						payload_swap( v1, &rbuf[2], size );
					} else {
						payload_swap( v1, &rbuf[2], (got-expected)/4 );
						st = range->write( &rbuf[2], size, off - range->base, debug );
					}
#ifdef DEBUG
					if (debug) {
						if ( cmd_rd )
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
			printf("No range matched 0x%08"PRIx64"\n", off);
		}
#endif
		if ( st ) {
			size=0;
		}
		memset( &rbuf[2+size], st ? 0xff : 0x00, 4);

		bs32(v1, rbuf[2+size] );

		bsize = (size + 3) * 4;

		if ( (put = udpPrtSend( port, &header, hsize, rbuf, bsize )) < 0 ) {
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
	return (void*)rval;
}

static void sh(int sn)
{
	fflush(stdout);
	fflush(stderr);
	exit(0);
}

struct srpvariant {
	int port, vers, haveRssi;
};

struct strmvariant {
	int port, haveRssi;
};

int
main(int argc, char **argv)
{
int      rval = 1;
int      opt;
int     *i_a;
const char *ina     = INA_DEF;
int      n_frags    = NFRAGS;
int      sim_loss   = SIMLOSS_DEF;
int      scramble   = SCRMBL_DEF;
unsigned tdest      = 0;
int      i;
sigset_t sigset;

#define V1_NORSSI 0
#define V2_NORSSI 1
#define V2_RSSI   2
struct srpvariant srpvars[] = {
	{ V1PORT_DEF,     1, WITHOUT_RSSI },
	{ V2PORT_DEF,     2, WITHOUT_RSSI },
	{ V2RSSIPORT_DEF, 2, WITH_RSSI    },
};

#define STRM_NORSSI 0
#define STRM_RSSI   1
struct strmvariant strmvars[] = {
	{ SPORT_DEF,     WITHOUT_RSSI },
	{ SRSSIPORT_DEF, WITH_RSSI    },
};

int    nprts;

struct streamer_args  strm_args[sizeof(strmvars)/sizeof(strmvars[0])];
struct srp_args       srp_args[sizeof(srpvars)/sizeof(srpvars[0])];

	memset(&srp_args,  0, sizeof(srp_args));
	memset(&strm_args, 0, sizeof(strm_args));

	signal( SIGINT, sh );

	while ( (opt = getopt(argc, argv, "dP:p:a:hs:f:S:L:r:R:")) > 0 ) {
		i_a = 0;
		switch ( opt ) {
			case 'h': usage(argv[0]);      return 0;

			case 'P': i_a = &srpvars[V2_NORSSI].port;       break;
			case 'p': i_a = &srpvars[V1_NORSSI].port;       break;
			case 'a': ina = optarg;                         break;
			case 'd': debug = 1;                            break;
			case 's': i_a = &strmvars[STRM_NORSSI].port;    break;
			case 'f': i_a = &n_frags;                       break;
			case 'L': i_a = &sim_loss;                      break;
			case 'S': i_a = &scramble;                      break;
			case 'r': i_a = &srpvars[V2_RSSI].port;         break;
			case 'R': i_a = &strmvars[STRM_RSSI].port;      break;
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

	nprts = 0;

	for ( i=0; i<sizeof(strmvars)/sizeof(strmvars[0]); i++ ) {
		if ( strmvars[i].port > 0 )
			nprts++;
	}
	for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
		if ( srpvars[i].port > 0 )
			nprts++;
	}

	if ( 0 == nprts ) {
		fprintf(stderr,"At least one port must be opened\n");
		goto bail;
	}

	for ( i=0; i<sizeof(strmvars)/sizeof(strmvars[0]); i++ ) {
		if ( strmvars[i].port <= 0 )
			continue;

		strm_args[i].port                 = udpPrtCreate( ina, strmvars[i].port, sim_loss, scramble, strmvars[i].haveRssi );
		strm_args[i].n_frags              = n_frags;
		strm_args[i].tdest                = tdest;
		strm_args[i].fram                 = 0;
		strm_args[i].jam                  = 0;

		if ( ! strm_args[i].port )
			goto bail;
		if ( pthread_create( &strm_args[i].poller_tid, 0, poller, (void*)&strm_args[i] ) ) {
			perror("pthread_create [poller]");
			goto bail;
		}
		if ( pthread_create( &strm_args[i].fragger_tid, 0, fragger, (void*)&strm_args[i] ) ) {
			void *res;
			perror("pthread_create [fragger]");
			pthread_cancel( strm_args[i].poller_tid );
			pthread_join( strm_args[i].poller_tid, &res );
			goto bail;
		}
		strm_args[i].haveThreads = 1;
	}

	for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
		if ( srpvars[i].port <= 0 )
			continue;
		srp_args[i].vers     = srpvars[i].vers;
		// don't scramble SRP - since SRP is synchronous (single request-response) it becomes extremely slow when scrambled
		srp_args[i].port     = udpPrtCreate( ina, srpvars[i].port, sim_loss, 0, srpvars[i].haveRssi );
		if ( pthread_create( &srp_args[i].tid, 0, srpHandler, (void*)&srp_args[i] ) ) {
			perror("pthread_create [srpHandler]");
			goto bail;
		}
		srp_args[i].haveThread = 1;
	}

	sigemptyset( &sigset );
	sigaddset( &sigset, SIGQUIT );
	sigaddset( &sigset, SIGINT  );
	sigwait( &sigset, &i );

bail:
	for ( i=0; i<sizeof(strmvars)/sizeof(strmvars[0]); i++ ) {
		void *res;
		if ( strm_args[i].haveThreads ) {
			pthread_cancel( strm_args[i].poller_tid );
			pthread_join( strm_args[i].poller_tid, &res );
			rval += (uintptr_t)res;
			pthread_cancel( strm_args[i].fragger_tid );
			pthread_join( strm_args[i].fragger_tid, &res );
			rval += (uintptr_t)res;
		}
		if ( strm_args[i].port )
			udpPrtDestroy( strm_args[i].port );
	}
	for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
		void *res;
		if ( srp_args[i].haveThread ) {
			pthread_cancel( srp_args[i].tid );
			pthread_join( srp_args[i].tid, &res );
			rval += (uintptr_t)res;
		}
		if ( srp_args[i].port )
			udpPrtDestroy( srp_args[i].port );
	}
	return rval;
}
