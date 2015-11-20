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
#define bs32(x) swp32(LE, x)

				fprintf(stderr, "usage: %s -a <inet_addr> -p <port>\n", argv[0]);

uint8_t mem[1024*1024] = {0};

static void usage(const char *nm)
{
	fprintf(stderr, "usage: %s -a <inet_addr> -p <port> [-1]\n", nm);
	fprintf(stderr, "        -1   : protocol version V1 (V2 default)\n");
}

int
main(int argc, char **argv)
{
int sd = -1;
int rval = 1;
struct   sockaddr_in me, you;
uint32_t rbuf[2048];
int      n, got;
uint32_t addr = 0;
uint32_t size = 16;
char *c_a;
int  *i_a;
int        port = 0;
const char *ina = 0;
socklen_t  youlen;
unsigned off;
int      v1 = 0;

	while ( (got = getopt(argc, argv, "p:a:1h")) > 0 ) {
		c_a = 0;
		i_a = 0;
		switch ( got ) {
			case 'h': usage(argv[0]); return 0;
			case 'p': i_a = &port;    break;
			case 'a': ina = optarg;   break;
			case '1': v1 = 1;         break;
			default:
				fprintf(stderr, "unknown option '%c'\n", got);
				usage(argv[0]);
				return 1;
		}
		if ( i_a ) {
			if ( 1 != sscanf(optarg,"%i",i_a) ) {
				fprintf(stderr,"Unable to scan option '-%c' argument '%s'\n", opt, optarg);
				return 1;
			}
		}
	}

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

	while ( 1 ) {

		youlen = sizeof(you);
		got = recvfrom(sd, (void*)rbuf, sizeof(rbuf), 0, (struct sockaddr*)&you, &youlen );
		if ( got < 0 ) {
			perror("read");
			goto bail;
		}

		addr = bs32(  rbuf[1] );

		off = CMD_ADDR(addr);

		if ( CMD_IS_RD(addr) ) {
			size = bs32(  rbuf[2] ) + 1;
		} else {
			size = (got - 12)/4;
		}

		if ( off + 4*size > sizeof(mem) ) {
			fprintf(stderr,"%s request out of range (off %d, size %d)\n", CMD_IS_RD(addr) ? "read" : "write", off, size);
			size=0;
			memset( &rbuf[2+size], 0xff, 4);
		} else {
			if ( CMD_IS_RD(addr) ) {
				memcpy((void*) &rbuf[2], mem+off, size*4);
			} else {
				memcpy(mem + off, (void*)&rbuf[2], got-12);
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
		bs32( rbuf[2+size] );

		if ( sendto( sd, (void*)rbuf, (size+3)*4, 0, (struct sockaddr*)&you, sizeof(you) ) < 0 ) {
			perror("unable to send");
			goto bail;
		}

	}

	rval = 0;
bail:
	if ( sd >= 0 )
		close( sd );
	return rval;
}
