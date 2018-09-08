/*@C Copyright Notice
 *@C ================
 *@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 *@C file found in the top-level directory of this distribution and at
 *@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 *@C
 *@C No part of CPSW, including this file, may be copied, modified, propagated, or
 *@C distributed except according to the terms contained in the LICENSE.txt file.
 *@C*/

/* SOCKS4 and SOCKS5 client implementation (partial). This is mostly aimed at the ssh proxy; hence
 * there is no support for authentication methods.
 *
 * Since the API is designed to be a plug-in replacement for 'connect' we don't use the DNS
 * lookup features (have the proxy server resolve hostnames) of SOCKS4a and SOCKS5.
 */

/* Author: Till Straumann <strauman@slac.stanford.edu> */

#include <string.h>
#include <stdlib.h>

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>

#include "libSocksUtil.h"

#define SOCKS_NEGO_TIMEOUT  5 /* seconds */

static int
xferBuf(int sd, int do_rd, uint8_t *buf, int len, const char *pre)
{
int             xfr;
const char      *m1;
fd_set          efds, ifds, ofds;
struct timeval  tout;
struct timespec now, fut;

	FD_ZERO( &efds );
	FD_ZERO( &ifds );
	FD_ZERO( &ofds );

	FD_SET ( sd, &efds );

	if ( do_rd ) {
		FD_SET( sd, &ifds );
		m1   = "read";
	} else {
		FD_SET( sd, &ofds );
		m1   = "write";
	}

	if ( clock_gettime( CLOCK_REALTIME, &now ) )
		return -1;

	fut.tv_sec  = now.tv_sec + SOCKS_NEGO_TIMEOUT;
	fut.tv_nsec = now.tv_nsec;

	do {

		/* calculate new timeout */
		if ( fut.tv_nsec < now.tv_nsec ) {
			now.tv_nsec -= 1000000000;
			now.tv_sec++;
		}
	
		tout.tv_usec      = (fut.tv_nsec - now.tv_nsec)/1000;
		tout.tv_sec       =  fut.tv_sec  - now.tv_sec;

		if ( (int)tout.tv_sec < 0 ) {
			return -1;
			/* timed out */
		}

		/* xfer progress ? */
		if ( select( sd + 1, &ifds, &ofds, &efds, &tout ) < 0 ) {
			fprintf( stderr, "%s: select failed: '%s'\n", pre, strerror( errno ) );
			return -1;
		}

		xfr = do_rd ? recv( sd, buf, len, 0 ) : send( sd, buf, len, 0);

		if ( xfr <= 0 ) {
			if ( xfr < 0 ) {
				fprintf( stderr, "%s: %s failed: '%s'\n", pre, m1, strerror( errno ) );
			} else {
				fprintf( stderr, "%s: %s incomplete (%i missing).\n", pre, m1, len);
			}
			return -1;
		}

		len -= xfr;

		if ( len <= 0 )
			break;

		/* more to do; prepare for calculating new timeout */

		buf += xfr;
		if ( clock_gettime( CLOCK_REALTIME, &now ) )
			return -1;
		
	} while ( 1 );

	return 0;
}

int
libSocksSrcBuf(int sd, void *buf, int len, const char *pre)
{
	return xferBuf( sd, 1, buf, len, pre );
}

int
libSocksSinkBuf(int sd, void *buf, int len, const char *pre)
{
	return xferBuf( sd, 0, buf, len, pre );
}
