#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>

#define CMD_MSK    (0x3fffffff)
#define CMD_WR(a)  (0x40000000 | (((a)>>2)&CMD_MSK))
#define CMD_RD(a)  (0x00000000 | (((a)>>2)&CMD_MSK))

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
		return bswap32(x);
}

// protocol wants LE
#define bs32(x) swp32(LE, x)

static void
usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-o addr] [-s size] [-a ip_addr_string] [-p port] -1\n", nm);
	fprintf(stderr,"       -1  -- protocol version 1\n");
}

int
main(int argc, char **argv)
{
int sd = -1;
int rval = 1;
struct sockaddr_in me, you;
uint32_t tbuf[100];
uint8_t  rbuf[2048];
int      n, got;
uint32_t addr = 0;
uint32_t size = 16;
uint32_t port = 8192;
int      v1   = 0;
int      opt;
uint32_t *i_p;
char    *ipa  = "192.168.2.10";

	while ( (opt = getopt(argc, argv, "o:s:a:p:")) > 0 ) {
		i_p = 0;
		switch ( opt ) {
			case 'o': i_p = &addr; break;
			case 's': i_p = &size; break;
			case 'p': i_p = &port; break;
			case '1': v1 = 1;      break;
			case 'a': ipa = optarg; break;
			case 'h': usage(argv[0]); return 0;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				return 1;
		}
		if ( i_p && 1 != sscanf(optarg, "%"SCNi32, i_p) ) {
			fprintf(stderr,"Unable to scan (int) arg to '-%c'\n", opt);
			goto bail;
		}
	}

	if ( (sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket");
		goto bail;
	}

	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port   = 0;

	you.sin_family = AF_INET;
	you.sin_addr.s_addr = inet_addr(ipa);
	you.sin_port   = htons( (short)port );

	if ( bind( sd, (struct sockaddr*)&me, sizeof(me) ) < 0 ) {
		perror("bind");
		goto bail;
	}
	n = 0;
	tbuf[n++] = bs32(  0 );
	tbuf[n++] = bs32( CMD_RD( addr ) );
	tbuf[n++] = bs32( size-1 );
	tbuf[n++] = bs32(  0 );

	if ( sendto( sd, (void*)tbuf, sizeof(tbuf[0])*n, 0, (struct sockaddr*)&you, sizeof(you) ) < 0 ) {
		perror("unable to send");
		goto bail;
	}

	if ( (got = recv( sd, (void*)rbuf, sizeof(rbuf), 0)) < 0 ) {
		perror("receive error");
	} else {
		for ( n=0; n<got; ) {
			printf("%02x ", rbuf[n]);
			if ( ++n % 16 == 0 )
				printf("\n");
		}
		printf("\n");
		
	}
	

	rval = 0;
bail:
	if ( sd >= 0 )
		close( sd );
	return rval;
}
