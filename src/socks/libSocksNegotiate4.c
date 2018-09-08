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
#include <sys/socket.h>
#include <netinet/in.h>

#include "libSocks.h"

#include "libSocksUtil.h"

/* Constants defining the SOCKS protocol V4 */

#define SOCKS4_VERS                  0x04

#define SOCKS4_CMD_CONN              0x01
#define SOCKS4_CMD_BIND              0x02

#define SOCKS4_STA_GRANTED           0x5a
#define SOCKS4_STA_FAILED            0x5b
#define SOCKS4_STA_FAILED_NO_IDENTD  0x5c
#define SOCKS4_STA_FAILED_IDENTD     0x5d

/* length of request not including username */
#define SOCKS4_REQ_LEN                  8
#define SOCKS4_REP_LEN                  8

#define SOCKS_NULL                   0x00

typedef struct Socks4Req_ {
	uint8_t	  vers;
	uint8_t    cmnd;
	uint16_t   port;
	uint32_t   addr;
	uint8_t    null;
	uint8_t    unresolved_remote[]; /* Socks4a could send an unresolved host name here */
} Socks4Req;

typedef struct Socks4Rep_ {
	uint8_t	   null;
	uint8_t    stat;
	uint8_t    unused[6];
} Socks4Rep;

typedef union Socks4ReqU_ {
	Socks4Req  s4;
	char       raw[sizeof(Socks4Req)];
} Socks4ReqU;

typedef union Socks4RepU_ {
	Socks4Rep  s4;
	char       raw[sizeof(Socks4Rep)];
} Socks4RepU;

int
libSocksNegotiateV4(int sockfd, const struct sockaddr *addr)
{
Socks4ReqU req;
Socks4RepU rep;
int        len;

	if ( AF_INET != addr->sa_family )
		return -1;

	req.s4.vers = SOCKS4_VERS;
	req.s4.cmnd = SOCKS4_CMD_CONN;
	/* already in network byte order */
	req.s4.port = ((struct sockaddr_in*)addr)->sin_port;
	req.s4.addr = ((struct sockaddr_in*)addr)->sin_addr.s_addr;
	req.s4.null = SOCKS_NULL;

	/* dont use sizeof(Socks4Req) because it is probably padded */
	len = SOCKS4_REQ_LEN + 1; /* terminating NULL */

	if ( libSocksSinkBuf( sockfd, req.raw, len, "socks4Negotiate") )
		return -1;

	if ( libSocksSrcBuf( sockfd, rep.raw, SOCKS4_REP_LEN, "socks4Negotiate" ) )
		return -1;

	if ( SOCKS_NULL != rep.s4.null ) {
		fprintf( stderr, "socks4Negotiate: unexpected reply; first octet is non-zero\n" );
		return -1;
	}
	
	switch ( rep.s4.stat ) {
		case SOCKS4_STA_GRANTED:
		return 0; /* SUCCESS */

		case SOCKS4_STA_FAILED:
			fprintf( stderr, "socks4Negotiate: SOCKS4 server denied CONNECT request\n" );
		break;

		case SOCKS4_STA_FAILED_NO_IDENTD:
			fprintf( stderr, "socks4Negotiate: SOCKS4 server - no identd\n" );
		break;

		case SOCKS4_STA_FAILED_IDENTD:
			fprintf( stderr, "socks4Negotiate: SOCKS4 server - identd failure\n" );
		break;
		
		default:
			fprintf( stderr, "socks4Negotiate: SOCKS4 server - UNKNOWN STATUS: %u\n", rep.s4.stat );
		break;
	}
	
	return -1;
}
