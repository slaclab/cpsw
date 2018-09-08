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
#include <stdio.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "libSocks.h"

static const char *
skipw(const char *str)
{
	while ( *str && isspace(*str) )
		str++;
	return str;
}

int
libSocksStrToProxy(LibSocksProxy *proxy, const char *name)
{
char           *dup = 0;
char           *col;
const char     *str;
int             rval = -1;
struct addrinfo hint, *aires = 0;
int             err;

	proxy->version = -1; /* no proxy */

	if ( ! name )
		return 0;

	name = skipw( name );

	/* empty string -> no proxy */
	if ( ! *name )
		return 0;

	if ( ! ( dup = strdup( name ) ) )
		return -1;

	str = dup;


	if ( 0 == strncmp(str, "socks4:", strlen("socks4:")) ) {
		proxy->version = 4;
		str = skipw(str + strlen("socks4:"));
	} else if ( 0 == strncmp(str, "socks5:", strlen("socks5:")) ) { 
		proxy->version = 5;
		str = skipw(str + strlen("socks5:"));
	}
	if ( proxy->version >= 0 ) {
		if ( 0 == strncmp(str, "//", 2) )
			str+=2;
	} else {
		proxy->version = 5; /* default version */
	}

	if ( ! *str )
		goto bail;

	if ( (col = strrchr( str, ':' )) ) {
		*col = 0;
		col++;
	} else {
		col = "1080"; /* default port */
	}

	memset( &hint, 0, sizeof( hint ) );
	hint.ai_family   = AF_INET;
	hint.ai_socktype = SOCK_STREAM;

	if ( (err = getaddrinfo( str, col, &hint, &aires )) ) {
		fprintf( stderr, "ERROR by getaddrinfo: %s\n", gai_strerror( err ) );
		goto bail;
	}

	proxy->address.sa = *aires->ai_addr;

	rval = 0;

bail:
	free( dup );
	if ( aires )
		freeaddrinfo( aires );
	return rval;
}
