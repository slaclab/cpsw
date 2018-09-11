 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_SOCK_WRAPPER_H
#define CPSW_SOCK_WRAPPER_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

class CSockSd;
typedef shared_ptr<CSockSd> SockSd;

struct LibSocksProxy;

class CSockSd {
private:
	int            sd_;
	int            type_;
	LibSocksProxy *proxy_;

	CSockSd & operator=(CSockSd &orig);

public:

	CSockSd(int type = SOCK_DGRAM, LibSocksProxy *proxy = 0);

	CSockSd(CSockSd &orig);

	virtual void getMyAddr(struct sockaddr_in *addr_p);

	virtual ~CSockSd();

	virtual int getSd() const { return sd_; }

	virtual void init(struct sockaddr_in *dst, struct sockaddr_in *me, bool nonBlocking);

	virtual int getMTU();
};

#endif
