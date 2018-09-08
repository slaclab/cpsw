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

/* inet6 stuff wouldn't compile under windows */
#define USE_INET6_SUPPORT

/* Constants defining the SOCKS protocol V5 */

#define SOCKS5_VERS                  0x05

#define SOCKS5_AUTH_NONE             0x00
#define SOCKS5_AUTH_GSSAPI           0x01
#define SOCKS5_AUTH_USRNAME          0x02

#define SOCKS5_CMD_CONN              0x01
#define SOCKS5_CMD_BIND              0x02
#define SOCKS5_CMD_ASSOC             0x03

#define SOCKS5_ADDR_IPV4             0x01
#define SOCKS5_ADDR_DNS              0x03
#define SOCKS5_ADDR_IPV6             0x04

#define SOCKS5_PRE_REQ_HDR_LEN          2 /* not including auth array */
#define SOCKS5_REQ_LEN                  6
#define SOCKS5_PRE_REP_LEN              2
#define SOCKS5_REP_HDR_LEN              4 /* not covering a DNS reply length */
#define SOCKS5_REP_LEN                 20 /* not covering a DNS reply length */

#define SOCKS5_STA_GRANTED           0x00
#define SOCKS5_STA_FAILED            0x01
#define SOCKS5_STA_RULEVIOLATION     0x02
#define SOCKS5_STA_NETUNREACH        0x03
#define SOCKS5_STA_HOSTUNREACH       0x04
#define SOCKS5_STA_CONNREFUSED       0x05
#define SOCKS5_STA_TTLEXPIRED        0x06
#define SOCKS5_STA_PROTOERR          0x07
#define SOCKS5_STA_ATYPEUNSUPP       0x08

#define SOCKS_NULL                   0x00

/* For now we only support 1 method: AUTH_NONE; in
 * the future, the array size may be increased.
 * Note that the ssh SOCKS proxy does currently
 * not support authentication.
 */
#define OUR_SOCKS5_N_METH 1

typedef struct Socks5PreReq_ {
	uint8_t    vers;
	uint8_t    nmet;
	uint8_t    meth[OUR_SOCKS5_N_METH];
} Socks5PreReq;

typedef struct Socks5PreRep_ {
	uint8_t    vers;
	uint8_t    meth;
} Socks5PreRep;

/* NOTE: for transferring a BIND address (name) this struct is not big enough!
 *       space for the 'payload' (name + port-no) must be tacked on.
 */
typedef struct Socks5ReqHdr_ {
	uint8_t    vers;
	uint8_t    cmnd;
	uint8_t    rsvd;
	uint8_t    atyp;
	union {
		struct {
			uint32_t addr;
			uint16_t port;
		}      ipv4;
		struct {
			uint8_t  addr[16];
			uint16_t port;
		}      ipv6;
		struct {
			uint8_t  alen;
			uint8_t  addr[]; /* exact format of address depends on atyp */
		}      bind;
	}          addr;
} Socks5ReqHdr;

/* We don't need BIND since our API has the caller pass in a
 * (resolved) address.
 * We already have 15 bytes (the union uses 16 bytes due to ipv6).
 * and the port; BIND_EXTRA_SPACE is on top of these.
 */
#define BIND_EXTRA_SPACE 0

typedef union Socks5ReqU_ {
	Socks5ReqHdr  s5;
	uint8_t       raw[sizeof(Socks5ReqHdr) + BIND_EXTRA_SPACE];
} Socks5ReqU;

typedef struct Socks5RepHdr_ {
	uint8_t       vers;
	uint8_t       rply;
	uint8_t       rsvd;
	uint8_t       atyp;
} Socks5RepHdr;

int
libSocksNegotiateV5(int sockfd, const struct sockaddr *addr)
{
Socks5PreReq preReq;
Socks5PreRep preRep;
Socks5ReqU   req;
Socks5RepHdr rep;
int          len, chunk;
uint8_t      aln;

	
	if ( AF_INET != addr->sa_family
#ifdef USE_INET6_SUPPORT
		&& AF_INET6 != addr->sa_family
#endif
	   ) {
		/* unsupported address */
		return -1;
	}

	preReq.vers    = SOCKS5_VERS;
	preReq.nmet    = OUR_SOCKS5_N_METH;
	preReq.meth[0] = SOCKS5_AUTH_NONE;

	/* Don't use sizeof() to compute length; structs are probably padded */
	if ( libSocksSinkBuf( sockfd, &preReq, SOCKS5_PRE_REQ_HDR_LEN + preReq.nmet, "socks5Negotiate (stage 1: auth methods)") )
		return -1;
	
	/* Don't use sizeof() to compute length; structs are probably padded */
	if ( libSocksSrcBuf( sockfd, &preRep, SOCKS5_PRE_REP_LEN, "socks5Negotiate (stage 1: auth methods)") )
		return -1;

	if ( SOCKS5_VERS != preRep.vers ) {
		/* fail silently in case they want to try SOCKS4 */
#define silent 0
		if ( !silent )
			fprintf( stderr, "socks5Negotiate: unexpected version %i received\n", preRep.vers );
		return -1;
	}

	if ( SOCKS5_AUTH_NONE != preRep.meth ) {
		fprintf( stderr, "socks5Negotiate: AUTH_NONE rejected by proxy server (but we don't support authentication)\n" );
		return -1;
	}

	/* Now we're ready to send a real request */
	req.s5.vers = SOCKS5_VERS;
	req.s5.cmnd = SOCKS5_CMD_CONN;
	req.s5.rsvd = SOCKS_NULL;

	/* includes the port number */
	len = SOCKS5_REQ_LEN;

	/* compute size in bytes; don't rely on struct layout which may be padded */
	if ( AF_INET == addr->sa_family ) {
		req.s5.atyp = SOCKS5_ADDR_IPV4;
		req.s5.addr.ipv4.addr = ((struct sockaddr_in*)addr)->sin_addr.s_addr;
		req.s5.addr.ipv4.port = ((struct sockaddr_in*)addr)->sin_port;
		len += 4;
	}
#ifdef USE_INET6_SUPPORT
	else { /* must be AF_INET6 as we already verified */
		req.s5.atyp = SOCKS5_ADDR_IPV6;
		/* address already in network byte order */
		memcpy( req.s5.addr.ipv6.addr, ((struct sockaddr_in6*)addr)->sin6_addr.s6_addr, 16 );
		req.s5.addr.ipv6.port = ((struct sockaddr_in6*)addr)->sin6_port;
		len += 16;
	}
#endif

	if ( libSocksSinkBuf( sockfd, req.raw, len, "socks5Negotiate (sending request)" ) )
		return -1;

	/* slurp the header first -- don't use 'sizeof' in case the struct is padded */
	if ( libSocksSrcBuf( sockfd, &rep, SOCKS5_REP_HDR_LEN, "socks5Negotiate (receiving reply [header])" ) )
		return -1;

	/* verify */
	if ( SOCKS5_VERS != rep.vers ) {
		fprintf( stderr, "socks5Negotiate: server replied with version != 5 (%i) ??\n", rep.vers );
		return -1;
	}

	switch ( rep.rply ) {
		case SOCKS5_STA_GRANTED:
			break;

		case SOCKS5_STA_FAILED           :
			fprintf( stderr, "socks5Negotiate: SOCKS5 general failure\n" );
			return -1;

		case SOCKS5_STA_RULEVIOLATION    :
			fprintf( stderr, "socks5Negotiate: SOCKS5 connection not allowed by ruleset\n" );
			return -1;

		case SOCKS5_STA_NETUNREACH       :
			fprintf( stderr, "socks5Negotiate: SOCKS5 network unreachable\n" );
			return -1;

		case SOCKS5_STA_HOSTUNREACH      :
			fprintf( stderr, "socks5Negotiate: SOCKS5 host unreachable\n" );
			return -1;

		case SOCKS5_STA_CONNREFUSED      :
			fprintf( stderr, "socks5Negotiate: SOCKS5 connection refused by destination host\n" );
			return -1;

		case SOCKS5_STA_TTLEXPIRED       :
			fprintf( stderr, "socks5Negotiate: SOCKS5 TTL expired\n" );
			return -1;

		case SOCKS5_STA_PROTOERR         :
			fprintf( stderr, "socks5Negotiate: SOCKS5 command not supported / protocol error\n" );
			return -1;

		case SOCKS5_STA_ATYPEUNSUPP      :
			fprintf( stderr, "socks5Negotiate: SOCKS5 address type not supported\n" );
			return -1;

		default:
			fprintf( stderr, "socks5Negotiate: SOCKS5 unknown status error: %i\n", rep.rply );
			return -1;
	}

	if ( SOCKS_NULL != rep.rsvd ) {
		fprintf( stderr, "socks5Negotiate: malformed reply - reserved byte is %i (not 0x00)\n", rep.rsvd);
		return -1;
	}

	len = 2; /* port number */

	switch ( rep.atyp ) {
		default:
			fprintf( stderr, "socks5Negotiate: malformed reply - unknown address type %i\n", rep.atyp);
			return -1;

		case SOCKS5_ADDR_IPV4: len +=  4;
			break;
		case SOCKS5_ADDR_IPV6: len += 16;
			break;

		case SOCKS5_ADDR_DNS:
			if ( libSocksSrcBuf( sockfd, &aln, 1, "socks5Negotiate: getting DNS address length" ) )
				return -1;
			len += aln;
			break;
	}

	/* slurp in the address info - just to clear the channel; we don't need it */
	while ( len > 0 ) {
		chunk = sizeof(rep);
		if ( len < chunk )
			chunk = len;
		libSocksSrcBuf( sockfd, req.raw, chunk, "socks5Negotiate: clearing channel" );

		len -= chunk;
	}

	return 0;
}
