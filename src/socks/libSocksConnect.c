/*@C Copyright Notice
 *@C ================
 *@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 *@C file found in the top-level directory of this distribution and at
 *@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 *@C
 *@C No part of CPSW, including this file, may be copied, modified, propagated, or
 *@C distributed except according to the terms contained in the LICENSE.txt file.
 *@C*/

/* Author: Till Straumann <strauman@slac.stanford.edu> */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include "libSocks.h"

int
libSocksDebug = 0;

int
libSocksConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, const LibSocksProxy *proxy)
{
int rval;
const struct sockaddr *dest = addr;

int vers = -1;

	if ( proxy ) {
		vers = proxy->version;
		if ( 4 == vers || 5 == vers ) {
			dest = &proxy->address.sa;

			if ( libSocksDebug > 0 ) {
				char bufs[100];
				char bufp[100];
				const struct sockaddr_in *sina = (const struct sockaddr_in*)addr;
				const struct sockaddr_in *sinp = (const struct sockaddr_in*)&proxy->address.sa;


				if (   inet_ntop( AF_INET, &sina->sin_addr, bufs, sizeof(bufs) )
						&& inet_ntop( AF_INET, &sinp->sin_addr, bufp, sizeof(bufp) ) ) {
					fprintf(stderr,"INFO: Connecting to %s:%d via %s:%d\n", bufs, ntohs(sina->sin_port), bufp, ntohs(sinp->sin_port));
				} else {
					fprintf(stderr,"WARNING (unable to print INFO message): inet_ntop failed: %s\n", strerror(errno));
				}
			}
		}
	}

	/* Make connection */
	if ( (rval = connect( sockfd, dest, sizeof( *dest ) )) ) {
		fprintf( stderr, "libSocksConnect() -- Unable to connect%s: %s\n", dest == addr ? "" : " to proxy", strerror( errno ) );
		return rval;
	}

	switch ( vers ) {
		case 4:
			return libSocksNegotiateV4( sockfd, addr );
		case 5:
			return libSocksNegotiateV5( sockfd, addr );
		default:
			break;
	}
	return 0;
}
