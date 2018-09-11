 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.


#include <cpsw_sock.h>
#include <cpsw_error.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <socks/libSocks.h>

void
CSockSd::reset(void)
{
const LibSocksProxy *orig;
	/* This also can be used by the copy-constructor as it creates a
	 * new socket
	 */
	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
	if ( (orig = proxy_) ) {
		proxy_  = new LibSocksProxy( *orig );
	}
}

CSockSd::CSockSd(int type, const LibSocksProxy *proxy)
: sd_   ( -1    ),
  type_ ( type  ),
  proxy_( proxy )
{
	reset();
}


CSockSd::CSockSd(int type, const char *proxyDesc)
: sd_   ( -1    ),
  type_ ( type  ),
  proxy_( 0     )
{
LibSocksProxy proxy;

	if ( libSocksStrToProxy( &proxy, proxyDesc ) ) {
		std::string msg = std::string("libSocksStrToProxy: unable to parse '") + std::string(proxyDesc) + std::string("'");
		throw InvalidArgError( msg );
	}

	// 'reset' will make a copy
	proxy_ = &proxy;

	reset();
}


CSockSd::CSockSd(const CSockSd &orig)
: sd_   ( -1          ),
  type_ ( orig.type_  ),
  proxy_( orig.proxy_ )
{
	reset();
}

void CSockSd::getMyAddr(struct sockaddr_in *addr_p)
{
	socklen_t l = sizeof(*addr_p);
	if ( getsockname( getSd(), (struct sockaddr*)addr_p, &l ) ) {
		throw IOError("getsockname() ", errno);
	}
}


int
CSockSd::getMTU()
{
int       mtu;
socklen_t sl = sizeof(mtu);
	if ( getsockopt( sd_, SOL_IP, IP_MTU, &mtu, &sl ) ) {
		throw InternalError("getsockopt(IP_MTU) failed", errno);
	}
	return mtu;
}

void CSockSd::init(struct sockaddr_in *dest, struct sockaddr_in *me_p, bool nblk)
{
int                optval;
struct sockaddr_in me;

	if ( NULL == me_p ) {
		me.sin_family      = AF_INET;
		me.sin_addr.s_addr = INADDR_ANY;
		me.sin_port        = htons( 0 );

		me_p = &me;
	}

	if ( nblk ) {
		if ( ::fcntl( sd_, F_SETFL, O_NONBLOCK ) ) {
			throw IOError("fcntl(O_NONBLOCK) ", errno);
		}
	}

	optval = 1;
	if ( ::setsockopt(  sd_,  SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) ) {
		throw IOError("setsockopt(SO_REUSEADDR) ", errno);
	}

	optval = 1;
	if ( SOCK_STREAM == type_ ) {
		if ( ::setsockopt( sd_,  IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval) ) ) {
			throw IOError("setsockopt(TCP_NODELAY) ", errno);
		}
	}

	if ( ::bind( sd_, (struct sockaddr*)me_p, sizeof(*me_p)) ) {
		throw IOError("bind failed ", errno);
	}

	// connect - filters any traffic from other destinations/fpgas in the kernel
	if ( dest ) {
		const LibSocksProxy *proxy = ( SOCK_STREAM == type_ ? proxy_ : 0 );
		if ( libSocksConnect( sd_, (struct sockaddr*)dest, sizeof(*dest), proxy ) )
			throw IOError("connect failed ", errno);
	}
}

CSockSd::~CSockSd()
{
	if ( proxy_ ) {
		delete proxy_;
	}
	close( sd_ );
}
