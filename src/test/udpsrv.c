/*
 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
*/

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

/* Must be longer than the client's poll interval (default 60 ATM) */
#define STREAM_POLL_SECS 80

#define V3VERS 3

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

#define V1PORT_DEF        8191
#define V2PORT_DEF        8192
#define V3PORT_DEF        8190
#define V3BPORT_DEF       8189
#define V3RSSIPORT_DEF    8188
#define V2RSSIPORT_DEF    8202
#define SPORT_DEF         8193
#define SRSSIPORT_DEF     8203
#define SRSSIPORT_V3_DEF  8200
#define SCRMBL_DEF           1
#define INA_DEF           "127.0.0.1"
#define SIMLOSS_DEF          1
#define NFRAGS_DEF        NFRAGS

#define TDEST_DEF            0 /* SRP TDEST is TDEST_DEF + 1 */

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
	fprintf(stderr, "        -T <port>   : run in TUTORIAL mode on <port>; use RSSI, SRPV3 and\n");
	fprintf(stderr, "                      ignore all other settings\n");
	fprintf(stderr, " Defaults: -a %s -P %d -p %d -s %d -f %d -L %d -S %d\n", INA_DEF, V2PORT_DEF, V1PORT_DEF, SPORT_DEF, NFRAGS_DEF, SIMLOSS_DEF, SCRMBL_DEF);
	fprintf(stderr, "           V3 (norssi                 ) is listening at  %d\n", V3PORT_DEF);
	fprintf(stderr, "           V3 (norssi, byte-resolution) is listening at %d\n",  V3BPORT_DEF);
	fprintf(stderr, "           V3 (rssi                   ) is listening at %d\n",  V3RSSIPORT_DEF);
	fprintf(stderr, "           V3@TDEST%d (rssi,   stream @TDEST%d) is listening at %d\n", TDEST_DEF+1,  TDEST_DEF, SRSSIPORT_V3_DEF);
	fprintf(stderr, "           V2@TDEST%d (rssi,   stream @TDEST%d) is listening at %d\n", TDEST_DEF+1,  TDEST_DEF, SRSSIPORT_DEF);
}

static void payload_swap(int v1, uint32_t *buf, int nelms)
{
int i;
	if ( ! v1 )
		return;
	for ( i=0; i<nelms; i++ )
		buf[i] = __builtin_bswap32( buf[i] );
}

#define SRP_OPT_BYTERES (1<<0)

typedef struct streamer_args {
	IoPrt             port;
	IoQue             srpQ;
	int                polled_stream_up;
	unsigned           n_frags;
	unsigned           fram;
	unsigned           tdest;
    int                rssi;
	unsigned           jam;
	int                srp_vers;
	unsigned           srp_opts;
	unsigned           srp_tdest;
	pthread_t          poller_tid, fragger_tid;
	int                haveThreads;
	volatile uint8_t   isRunning;
} streamer_args;

typedef struct SrpArgs {
	IoPrt             port;
	int                vers;
	unsigned           opts;
	pthread_t          tid;
	int                haveThread;
} SrpArgs;

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

static int handleSRP(int vers, unsigned opts, uint32_t *rbuf, unsigned rbufsz, int got);

void *poller(void *arg)
{
struct streamer_args *sa = (struct streamer_args*)arg;
uint8_t       buf[1500];
int           got;
int           lfram = -1;
struct timespec to;


	while ( 1 ) {

		do {
			clock_gettime( CLOCK_REALTIME, &to );

			to.tv_sec  += STREAM_POLL_SECS;

			got = ioPrtRecv( sa->port, buf, sizeof(buf), &to);

			if ( got < 0 )
				sa->polled_stream_up = 0;

		} while ( got < 0 );

		if ( got < 4 ) {
			fprintf(stderr,"Poller: Contacted by %d\n", ioPrtIsConn( sa->port ) );
			sa->polled_stream_up = 1;
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
			if ( debug ) {
				printf("Got stream message:\n");
				printf("   Version %d -- Frame #%d, frag #%d, tdest 0x%02x, tid 0x%02x, tusr1 0x%02x\n",
						vers,
						fram,
						frag,
						tdest,
						tid,
						tusr1);
			}
#endif

			if ( vers != 0 || frag != 0 || ( tdest != 0 && (0 == sa->srp_vers || sa->srp_tdest != tdest ) ) || tid != 0 || tusr1 != (frag == 0 ? (1<<1) : 0) ) {
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

			if ( sa->srp_vers && sa->srp_tdest == tdest ) {
				if ( got < 9 + 12 ) {
					fprintf(stderr,"UDPSRV: SRP request too small\n");
				} else {
					int put = ioQueTrySend( sa->srpQ, buf, got - 1 );

					if ( put < 0 )
						fprintf(stderr,"queueing SRP message failed\n");
#ifdef DEBUG
					else if ( debug )
						printf("Queued %d octets to SRP\n", put);
#endif
				}
			} else {

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

				sa->isRunning = !!(buf[8] & 1);
#ifdef DEBUG
				if ( debug ) {
					printf("Stream start/stop msg 0x%02"PRIx8"\n", buf[8]);
				}
#endif
			}
		}

	}
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

		if ( 0 == frag ) {

			do {
				int      got;
				uint32_t rbuf[1000000];

				struct timespec to;

				clock_gettime( CLOCK_REALTIME, &to );

				to.tv_nsec += sa->isRunning ? 10000000 : 100000000;
				if ( to.tv_nsec >= 1000000000 ) {
					to.tv_nsec -= 1000000000;
					to.tv_sec  += 1;
				}

				while ( (got = ioQueRecv( sa->srpQ, rbuf, sizeof(rbuf), &to )) > 0 ) {

					int put;
					uint32_t *bufp  = bufp  = rbuf + 2;
					unsigned  bufsz = sizeof(rbuf) - 2*sizeof(rbuf[0]);

#ifdef DEBUG
					if ( debug )
						printf("Got %d SRP octets\n", got);
#endif

					unsigned tdest = ((uint8_t*)rbuf)[5];

					if ( tdest == sa->srp_tdest ) {
						if ( (put = handleSRP(sa->srp_vers, sa->srp_opts, bufp, bufsz - 1, got - 2*sizeof(rbuf[0]))) <= 0 ) {
							fprintf(stderr,"handleSRP ERROR (from fragger)\n");
						}
					} else {
						put = got;
					}

					unsigned frag = 0;

					while ( put > 0 ) {
						unsigned chunk = 1024 - 8 - 12; /* must be word-aligned */
						uint8_t  tail;
						uint8_t *tailp;
						uint8_t *hp   = ((uint8_t*)bufp) - 8;
						if ( put < chunk )
							chunk = put;
						put   -= chunk;
						tailp = ((uint8_t*)bufp)+chunk;
						tail  = *tailp; /* save */
						insert_header( hp, sa->fram, frag, tdest);
						append_tail( tailp, put > 0 ? 0 : 1 );
						if ( ioPrtSend( sa->port, hp, chunk + 9 ) < 0 )
							fprintf(stderr,"fragmenter: write error (sending SRP or STREAM reply)\n");
#ifdef DEBUG
						else if ( debug )
							printf("Sent to port %d\n", ioPrtIsConn( sa->port ));
#endif
						*tailp = tail;
						bufp  += chunk>>2;
						bufsz -= chunk;
						frag++;
					}
					sa->fram++;
				}
			} while ( ! sa->isRunning );

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

		if ( ioPrtSend( sa->port, bufmem, i ) < 0 ) {
			fprintf(stderr, "fragmenter: write error\n");
			break;
		}
#ifdef DEBUG
		if ( debug )
			printf("fragger sent %d[%d]!\n", sa->fram, frag);
#endif

		if ( ++frag >= sa->n_frags ) {
			frag = 0;
			crc  = -1;
			sa->fram++;
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

typedef enum { CMD_NOP, CMD_RD, CMD_WR, CMD_PWR } Cmd;

static int handleSRP(int vers, unsigned opts, uint32_t *rbuf, unsigned rbufsz, int got)
{
int      v1       = (vers == 1);
int      v3       = (vers == 3);
int      expected = v3 ?              20 : ( v1 ? 16 : 12 );
int      bsize;
#ifdef DEBUG
uint32_t xid;
#endif
uint32_t size;
uint64_t off;
uint32_t cmdwrd;
struct   udpsrv_range *range;
Cmd      cmd;
/*int      posted   = 0;*/
unsigned v3vers;
int      noexec   =  0;
int      ooff     =  2;
int      szoff    =  2;
int      st       =  0;
uint32_t status   =  0;

	if ( v3 ) {

		cmdwrd = bs32(v1,  rbuf[0] );
#ifdef DEBUG
		xid    = bs32(v1,  rbuf[1] );
#endif
		off    = (((uint64_t)bs32(v1, rbuf[3])) << 32) | bs32(v1,  rbuf[2]);
		switch ( ((cmdwrd >> 8) & 3) ) {
			case 0: cmd = CMD_RD;  break;
			case 1: cmd = CMD_WR;  break;
			case 2: cmd = CMD_PWR; break;
			case 3: cmd = CMD_NOP; break;
		}
		/*posted = (cmdwrd & (3<<8)) == 2 << 8;*/
		v3vers = (cmdwrd & 0xff);

		ooff   = 5;
		szoff  = 4;

		if ( v3vers != V3VERS ) {
			noexec  = 1;
			status |= (1<<11); // version mismatch
			cmdwrd  = (cmdwrd & ~0xff) | V3VERS;
		}

		if ( (opts & SRP_OPT_BYTERES) )
			cmdwrd |=   (1<<10);
		else
			cmdwrd &= ~ (1<<10);

		rbuf[0] = bs32(v1, cmdwrd);

	} else {
		uint32_t addr;
		int      hoff;

		if ( v1 ) {
			ooff  = 3;
			szoff = 3;
			hoff  = 1;
		} else {
			ooff  = 2;
			szoff = 2;
			hoff  = 0;
		}

#ifdef DEBUG
		xid    = bs32(v1,  rbuf[hoff + 0] );
#endif
		addr   = bs32(v1,  rbuf[hoff + 1] );

		off    = CMD_ADDR(addr);
		cmd    = CMD_IS_RD(addr) ? CMD_RD : CMD_WR;
	}

	if ( v3 || (CMD_RD == cmd) ) {
		size = CMD_NOP == cmd ? 0 : (bs32(v1,  rbuf[szoff] ) + 1);
		if ( !v3 )
			size *= 4;
	} else {
		size = (got - expected);
	}

	switch ( cmd ) {
		case CMD_RD:
			if ( got != expected + ( !v3 ? 4 : 0 ) /* status word */ ) {
				int j;
				printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"READ command -- got != expected + %d; dropping...\n", v3 ? 0 : 4);
				for ( j = 0; j<(got+3)/4; j++ )
					fprintf(stderr,"  0x%08x\n", rbuf[j]);
				return 0;
			}
			break;

		case CMD_WR:
		case CMD_PWR:
			if ( got < expected ) {
				printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"WRITE command -- got < expected; dropping...\n");
				return 0;
			} else if ( (got % 4) != 0 ) {
				printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"WRITE command -- got not a multiple of 4; dropping...\n");
				return 0;
			}
			if ( v3 ) {
				if ( ((size + 3) & ~3) != (got - expected) ) {
					fprintf(stderr,"V3 WRITE command -- size doesn't match - size %d, got %d\n", size, got);
					return 0;
				}
			}
			break;

		case CMD_NOP:
			if ( got != expected ) {
				printf("got %d, exp %d\n", got, expected);
				fprintf(stderr,"NOP command -- got != expected; dropping...\n");
				return 0;
			}
			noexec = 1;
			st     = 0;
			break;
	}

	if ( ! noexec ) {
		if ( ! (opts & SRP_OPT_BYTERES) ) {
			if ( (off & 3) ) {
				fprintf(stderr,"%s @0x%016"PRIx64" [0x%04x]: address not word-aligned\n", CMD_RD == cmd ? "Read" : "Write" , off, size);
				st     = -1;
				noexec = 1;
			}
			if ( (size & 3) ) {
				fprintf(stderr,"%s @0x%016"PRIx64" [0x%04x]: size not word-aligned\n", CMD_RD == cmd ? "Read" : "Write" , off, size);
				st     = -1;
				noexec = 1;
			}
		}
		if ( size > rbufsz ) {
			fprintf(stderr,"rbuf size too small (%d; requested %d)\n", rbufsz, size);
			st     = -1;
			noexec = 1;
		}
	}

	if ( ! noexec ) {
		for ( range = udpsrv_ranges; range; range=range->next ) {
			if ( off >= range->base && off < range->base + range->size ) {
				if ( off + size > range->base + range->size ) {
					fprintf(stderr,"%s request out of range (off 0x%"PRIx64", size %d)\n", CMD_RD == cmd ? "read" : "write", off, size);
					fprintf(stderr,"range base 0x%"PRIx64", size %"PRId64"\n", range->base, range->size);
				} else {
					if ( CMD_RD == cmd ) {
						st = range->read( (uint8_t*)&rbuf[ooff], size, off - range->base, debug );
						payload_swap( v1, &rbuf[ooff], (size + 3)/4 );
					} else {
						payload_swap( v1, &rbuf[ooff], (got-expected)/4 );
						st = range->write( (uint8_t*)&rbuf[ooff], size, off - range->base, debug );
					}
#ifdef DEBUG
					if (debug) {
						if ( CMD_RD == cmd )
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
	}
	if ( st ) {
		size    = 0;
		status |= 0xff;
	}

	unsigned nw = (size + 3)/4;

	rbuf[ooff+nw] = status;

	bs32(v1, rbuf[ooff+nw]);

	bsize = (ooff + nw + 1 /* footer */) * 4;

	return bsize;
}

static void* srpHandler(void *arg)
{
SrpArgs  *srp_arg = (SrpArgs*)arg;
uintptr_t rval = 1;
int      got, put;
uint32_t rbuf[300];
IoPrt   port     = srp_arg->port;
int      bsize;

	while ( 1 ) {

		got = ioPrtRecv( port, rbuf, sizeof(rbuf), 0 );
		if ( got < 0 ) {
			perror("read");
			goto bail;
		}
		
		bsize = handleSRP(srp_arg->vers, srp_arg->opts, rbuf, sizeof(rbuf), got);

		if ( bsize < 0 )
			goto bail;

		if ( bsize > 0 ) {
			if ( (put = ioPrtSend( port, rbuf, bsize )) < 0 ) {
				perror("unable to send");
				break;
			}
#ifdef DEBUG
			if ( debug )
				printf("put %i\n", put);
#endif
		}
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
	int port, vers, haveRssi, opts;
};

struct strmvariant {
	int port, haveRssi, srpvers;
};

#define V1_NORSSI  0
#define V2_NORSSI  1
#define V2_RSSI    2
#define V3_NORSSI  3
#define V3B_NORSSI 4
struct srpvariant srpvars[] = {
	{ V1PORT_DEF,     1, WITHOUT_RSSI,               0 },
	{ V2PORT_DEF,     2, WITHOUT_RSSI,               0 },
	{ V2RSSIPORT_DEF, 2, WITH_RSSI,                  0 },
	{ V3PORT_DEF,     3, WITHOUT_RSSI,               0 },
	{ V3BPORT_DEF,    3, WITHOUT_RSSI, SRP_OPT_BYTERES },
	{ V3RSSIPORT_DEF, 3, WITH_RSSI,                  0 },
};

#define STRM_NORSSI 0
#define STRM_RSSI   1
struct strmvariant strmvars[] = {
	{ SPORT_DEF,        WITHOUT_RSSI, 0 }, /* run no SRP on scrambled channel */
	{ SRSSIPORT_DEF,    WITH_RSSI,    2 }, /* SRP will be slow due to scrambled RSSI */
	{ SRSSIPORT_V3_DEF, WITH_RSSI,    3 }, /* SRP will be slow due to scrambled RSSI */
};

struct streamer_args  strm_args[sizeof(strmvars)/sizeof(strmvars[0])];
struct SrpArgs        srpArgs  [sizeof(srpvars)/sizeof(srpvars[0])  ];

/* send a stream message; the buffer must have 8-bytes at the head reserved for the packet header */
void
streamSend(uint8_t *buf, int size, uint8_t tdest)
{
int i;
	for ( i=0; i<sizeof(strm_args)/sizeof(strm_args[0]); i++ ) {
		if ( ! strm_args[i].port ) {
			continue;
		}
		if (  ! ioPrtRssiIsConn( strm_args[i].port ) && ! strm_args[i].polled_stream_up ) {
			continue;
		}
#ifdef DEBUG
		if ( debug ) {
			if ( ioPrtRssiIsConn( strm_args[i].port ) )
				printf("RSSI conn (chnl %d)\n", i);
			if ( strm_args[i].polled_stream_up )
				printf("Polled up (chnl %d)\n", i);
			if ( strm_args[i].isRunning )
				printf("Test stream is running (chnl %d)\n", i);
		}
#endif

		if ( strm_args[i].isRunning ) {
			// if the stream test is running then they look for consecutive
			// frame numbers - which we don't want to mess up...
			continue;
		}
		if ( tdest == strm_args[i].srp_tdest ) {
			fprintf(stderr,"ERROR: cannot post to stream on TDEST %d -- already used by SRP\n", tdest);
			exit(1);
		} else if ( tdest == strm_args[i].tdest ) {
			fprintf(stderr,"ERROR: cannot post to stream on TDEST %d -- already used by udpsrv's STREAM\n", tdest);
			exit(1);
		}

		buf[5] = tdest;

		if ( ioQueTrySend( strm_args[i].srpQ, buf, size ) < 0 ) {
			fprintf(stderr,"INFO: Stream message dropped\n");
		}
	}
}

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
unsigned tdest      = TDEST_DEF;
int      use_tcp    = 0;
int      i;
sigset_t sigset;
int      tut_port   = 0;

int      nprts, nstrms;

	memset(&srpArgs,  0, sizeof(srpArgs));
	memset(&strm_args, 0, sizeof(strm_args));

	signal( SIGINT, sh );

	while ( (opt = getopt(argc, argv, "dP:p:a:hs:f:S:L:r:R:tT:")) > 0 ) {
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
			case 'T': i_a = &tut_port;                      break;
			case 't': use_tcp = 1;                          break;
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

	if ( tut_port ) {
		if ( tut_port < 0 || tut_port > 65535 ) {
			fprintf(stderr,"Invalid -T argument (%d out of range)\n", tut_port);
			exit(1);
		}
		nprts++;
	} else {
		for ( i=0; i<sizeof(strmvars)/sizeof(strmvars[0]); i++ ) {
			if ( strmvars[i].port > 0 )
				nprts++;
		}
		for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
			if ( srpvars[i].port > 0 )
				nprts++;
		}
	}

	if ( 0 == nprts ) {
		fprintf(stderr,"At least one port must be opened\n");
		goto bail;
	}

	if ( tut_port ) {
		strmvars[0].port     = tut_port;
		strmvars[0].haveRssi = WITH_RSSI;
		strmvars[0].srpvers  = 3;
		sim_loss = 0;
		scramble = 0;
		nstrms   = 1;
	} else {
		nstrms = sizeof(strmvars)/sizeof(strmvars[0]);
	}

	if ( use_tcp ) {
		for ( i=0; i<nstrms; i++ )
			strmvars[i].haveRssi = WITHOUT_RSSI;
		for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
			srpvars[i].haveRssi = WITHOUT_RSSI;
		}
	}

	for ( i=0; i<nstrms; i++ ) {
		if ( strmvars[i].port <= 0 )
			continue;

		if ( use_tcp ) {
			strm_args[i].port             = tcpPrtCreate( ina, strmvars[i].port );
		} else {
			strm_args[i].port             = udpPrtCreate( ina, strmvars[i].port, sim_loss, scramble, strmvars[i].haveRssi );
		}
		strm_args[i].srpQ                 = ioQueCreate(10);
		strm_args[i].n_frags              = n_frags;
		strm_args[i].tdest                = tdest;
		strm_args[i].fram                 = 0;
		strm_args[i].jam                  = 0;
		strm_args[i].srp_vers             = strmvars[i].srpvers;
		strm_args[i].rssi                 = strmvars[i].haveRssi;
		strm_args[i].srp_tdest            = (tdest + 1) & 0xff;

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
		srpArgs[i].vers     = srpvars[i].vers;
		srpArgs[i].opts     = srpvars[i].opts;
		// don't scramble SRP - since SRP is synchronous (single request-response) it becomes extremely slow when scrambled
		if ( use_tcp ) {
			srpArgs[i].port = tcpPrtCreate( ina, srpvars[i].port );
		} else {
			srpArgs[i].port = udpPrtCreate( ina, srpvars[i].port, sim_loss, 0, srpvars[i].haveRssi );
		}
		if ( pthread_create( &srpArgs[i].tid, 0, srpHandler, (void*)&srpArgs[i] ) ) {
			perror("pthread_create [srpHandler]");
			goto bail;
		}
		srpArgs[i].haveThread = 1;
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
			ioPrtDestroy( strm_args[i].port );
		if ( strm_args[i].srpQ )
			ioQueDestroy( strm_args[i].srpQ );
	}
	for ( i=0; i<sizeof(srpvars)/sizeof(srpvars[0]); i++ ) {
		void *res;
		if ( srpArgs[i].haveThread ) {
			pthread_cancel( srpArgs[i].tid );
			pthread_join( srpArgs[i].tid, &res );
			rval += (uintptr_t)res;
		}
		if ( srpArgs[i].port )
			ioPrtDestroy( srpArgs[i].port );
	}
	return rval;
}
