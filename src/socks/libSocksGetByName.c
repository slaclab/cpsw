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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include "libSocks.h"

int
libSocksGetByName(const char *host, const char *service, struct sockaddr *res)
{
struct addrinfo hint, *aires = 0;
int             err;

	memset( &hint, 0, sizeof( hint ) );
	hint.ai_family   = AF_INET;
	hint.ai_socktype = SOCK_STREAM;

	if ( (err = getaddrinfo( host, service, &hint, &aires )) ) {
		goto bail;
	}

	*res = *aires->ai_addr;

bail:
	if ( aires )
		freeaddrinfo( aires );
	return err;
}
