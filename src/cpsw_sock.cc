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

CSockSd::SA::SA()
{
	sin_family      = AF_INET;
	sin_addr.s_addr = INADDR_ANY;
	sin_port        = htons( 0 );
}

CSockSd::SA::SA( const struct sockaddr_in &orig )
{
	sin_family = orig.sin_family;
    sin_addr   = orig.sin_addr;
    sin_port   = orig.sin_port;
}


void
CSockSd::reset()
{
const LibSocksProxy *origProxy;
SA                  *origDest;

	/* This also can be used by the copy-constructor as it creates a
	 * new socket
	 */

	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}

	if ( (origDest = dest_ ) ) {
		dest_   = new SA( *origDest );
	}

	if ( (origProxy = proxy_) ) {
		proxy_  = new LibSocksProxy( *origProxy );
	}

	if ( isConn_ ) {
		init( dest_, NULL, nblk_ );
	}

}

CSockSd::CSockSd(const CSockSd &orig)
: sd_    ( -1           ),
  type_  ( orig.type_   ),
  me_    ( orig.me_     ),
  dest_  ( orig.dest_   ),
  nblk_  ( orig.nblk_   ),
  isConn_( orig.isConn_ ),
  proxy_ ( orig.proxy_  )
{
	reset();
}

void CSockSd::reconnect()
{
	close( sd_ );
	if ( ( sd_ = ::socket( AF_INET, type_, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
	if ( isConn_ ) {
		init( dest_, NULL, nblk_ );
	}
}

CSockSd::CSockSd(int type, const LibSocksProxy *proxy)
: sd_    ( -1    ),
  type_  ( type  ),
  dest_  ( NULL  ),
  nblk_  ( false ),
  isConn_( false ),
  proxy_ ( proxy )
{
	reset();
}


CSockSd::CSockSd(int type, const char *proxyDesc)
: sd_    ( -1    ),
  type_  ( type  ),
  dest_  ( NULL  ),
  nblk_  ( false ),
  isConn_( false ),
  proxy_ ( 0     )
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

void
CSockSd::getMyAddr(struct sockaddr_in *addr_p)
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
	int    optval;

	if ( me_p ) {
		me_ = *me_p;
	}

	if ( (nblk_ = nblk) ) {
		if ( ::fcntl( sd_, F_SETFL, O_NONBLOCK ) ) {
			throw IOError("fcntl(O_NONBLOCK) ", errno);
		}
	}

	optval = 1;
	if ( ::setsockopt(  sd_,  SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) ) {
		throw IOError("setsockopt(SO_REUSEADDR) ", errno);
	}

#ifdef USE_TCP_NODELAY
	optval = 1;
	if ( SOCK_STREAM == type_ ) {
		if ( ::setsockopt( sd_,  IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval) ) ) {
			throw IOError("setsockopt(TCP_NODELAY) ", errno);
		}
	}
#endif

	if ( ::bind( sd_, (struct sockaddr*)&me_, sizeof(me_)) ) {
		throw IOError("bind failed ", errno);
	}

	// connect - filters any traffic from other destinations/fpgas in the kernel
	if ( dest ) {
		struct sockaddr_in *tmp = dest_;

		dest_ = new SA(*dest);
		// they may pass in dest_ which must not be deleted until now
		if ( tmp )
			delete tmp;
	}

	if ( dest_ ) {
		const LibSocksProxy *proxy = ( SOCK_STREAM == type_ ? proxy_ : 0 );
		if ( libSocksConnect( sd_, (struct sockaddr*)dest_, sizeof(*dest_), proxy ) )
			throw IOError("connect failed ", errno);
	}

	isConn_ = true;
}

CSockSd::~CSockSd()
{
	if ( proxy_ ) {
		delete proxy_;
	}
	close( sd_ );
	if ( dest_ )
		delete dest_;
}
