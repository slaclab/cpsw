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
#include <netinet/in.h>
#include <errno.h>

#include <libSocks.h>

int 
libSocksConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen, LibSocksProxy *proxy)
{
int rval;
const struct sockaddr *dest = addr;

int vers = -1;

	if ( proxy ) {
		vers = proxy->version;
		if ( 4 == vers || 5 == vers ) {
			dest = &proxy->address.sa;
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
