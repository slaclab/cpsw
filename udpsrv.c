#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

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


// byte swap ? 
#define bsl(x) (x)

#define BE 1
#define LE 0

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
	fprintf(stderr, "usage: %s -a <inet_addr> -p <port> [-V <protoVersion>]\n", nm);
	fprintf(stderr, "        -V version  : protocol version V1 (V2 default)\n");
}

static void payload_swap(int v1, uint32_t *buf, int nelms)
{
int i;
	if ( ! v1 )
		return;
	for ( i=0; i<nelms; i++ )
		buf[i] = __builtin_bswap32( buf[i] );
}

int
main(int argc, char **argv)
{
int sd = -1;
int rval = 1;
struct   sockaddr_in me, you;
uint32_t rbuf[2048];
int      n, got, put;
uint32_t addr = 0;
uint32_t size = 16;
char *c_a;
int  *i_a;
int        port = 8192;
const char *ina = "127.0.0.1";
socklen_t  youlen;
unsigned off;
int      vers = 2;
int      v1;
uint32_t header = 0;

struct msghdr mh;
struct iovec  iov[2];
int    niov = 0;
int    i;
int    expected;

	memset( &mh, 0, sizeof(mh) );

	sprintf(mem + 0x800, "Hello");

	while ( (got = getopt(argc, argv, "p:a:V:h")) > 0 ) {
		c_a = 0;
		i_a = 0;
		switch ( got ) {
			case 'h': usage(argv[0]); return 0;
			case 'p': i_a = &port;    break;
			case 'a': ina = optarg;   break;
			case 'V': i_a = &vers;    break;
			default:
				fprintf(stderr, "unknown option '%c'\n", got);
				usage(argv[0]);
				return 1;
		}
		if ( i_a ) {
			if ( 1 != sscanf(optarg,"%i",i_a) ) {
				fprintf(stderr,"Unable to scan option '-%c' argument '%s'\n", got, optarg);
				return 1;
			}
		}
	}

	if ( vers != 1 && vers != 2 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1 or 2\n", vers);
		return 1;
	}

	v1 = (vers == 1);

	if ( (sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket");
		goto bail;
	}

	if ( ! ina || ! port ) {
		fprintf(stderr,"Need inet and port args (-a and -p options)\n");
		return 1;
	}
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = inet_addr(ina);
	me.sin_port   = htons( (short)port );

	if ( bind( sd, (struct sockaddr*)&me, sizeof(me) ) < 0 ) {
		perror("bind");
		goto bail;
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
printf("Clearing\n");
#endif
						}
						memset( mem + REGBASE + REG_CLR_OFF, 0xaa, 4 );
					}
				}
			}
			memset((void*) &rbuf[2+size], 0, 4);

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
		bs32(v1, rbuf[2+size] );

		iov[niov].iov_len = (size+3)*4;

		if ( (put = sendmsg( sd, &mh, 0 )) < 0 ) {
			perror("unable to send");
			goto bail;
		}
printf("put %i\n", put);

	}

	rval = 0;
bail:
	if ( sd >= 0 )
		close( sd );
	return rval;
}
