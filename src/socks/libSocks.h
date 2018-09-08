/*@C Copyright Notice
 *@C ================
 *@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 *@C file found in the top-level directory of this distribution and at
 *@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 *@C
 *@C No part of CPSW, including this file, may be copied, modified, propagated, or
 *@C distributed except according to the terms contained in the LICENSE.txt file.
 *@C*/

#ifndef SOCKS_PROXY_CONNECT_LIB_H
#define SOCKS_PROXY_CONNECT_LIB_H

#include <sys/types.h>
#include <sys/socket.h>

/*
 * SOCKS4 and SOCKS5 client implementation (partial). This is mostly aimed at the ssh proxy; hence
 * there is no support for authentication methods.
 *
 * Since the API is designed to be a plug-in replacement for 'connect' we don't use the DNS
 * lookup features (have the proxy server resolve hostnames) of SOCKS4a and SOCKS5.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Negotiate with a SOCKS proxy. An open connection to the proxy
 * must exist on the socket descriptor 'sockfd'.
 * A connection to the destination (via the proxy) is established.
 *
 * RETURNS: 0 on success, nonzero on error (message printed to stdout)
 */

int
libSocksNegotiateV4(int sockfd, const struct sockaddr *destination);

int
libSocksNegotiateV5(int sockfd, const struct sockaddr *destination);

typedef struct {
	int    version; /* <0 (no proxy), 4 or 5 */
	union {
		struct sockaddr_in sin;
		struct sockaddr    sa;
	}      address;
} LibSocksProxy;

/*
 * Connect 'sockfd' to the destination address, possibly via a proxy.
 * If the proxy is NULL or has a version < 0 then a direct connection
 * is made.
 */

int
libSocksConnect(int sockfd, const struct sockaddr *destination, socklen_t addrlen, LibSocksProxy *viaProxy);

/*
 * Parse a string into a LibSocksProxy struct. The string is
 * has the form:
 *
 *  [ 'socks4:' | 'socks5:' [ '//' ] ] 'proxy' [ ':' 'port' ]
 *
 * where the protocol defaults to socks5 and the port to 1080.
 * An empty or NULL string means no proxy (version < 0).
 */

int
libSocksStrToProxy(LibSocksProxy *proxy, const char *str);

#ifdef __cplusplus
}
#endif

#endif
