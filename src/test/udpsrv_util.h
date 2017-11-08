 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef Q_HELPER_H
#define Q_HELPER_H

#include <boost/shared_ptr.hpp>
#include <boost/atomic.hpp>

// simplified implementation of buffers etc.
// for use by udpsrv to have a testbed which
// is independent from the main code base...

#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <cpsw_error.h>

#include <cpsw_api_timeout.h>

#include <cpsw_event.h>
#include <cpsw_buf.h>
#include <cpsw_mutex.h>

#include <stdio.h>


using boost::shared_ptr;
using boost::atomic;

class IProtoPort;
typedef shared_ptr<IProtoPort> ProtoPort;

class IProtoPort {
public:
	virtual BufChain pop(const CTimeout *)                 = 0;
	virtual BufChain tryPop()                              = 0;

	virtual IEventSource *getReadEventSource()             = 0;
	virtual IEventSource *getWriteEventSource()            = 0;

	virtual ProtoPort     getUpstreamPort()                = 0;

	virtual void attach(ProtoPort upstream)                = 0;

	virtual bool push(BufChain, const CTimeout *)          = 0;
	virtual bool tryPush(BufChain)                         = 0;

	virtual unsigned isConnected()                         = 0;

	virtual ~IProtoPort() {}

	virtual void start()                                   = 0;
	virtual void stop()                                    = 0;

	static ProtoPort create(bool isServer_);
};

class IUdpPort;
typedef shared_ptr<IUdpPort> UdpPort;

class IUdpPort : public IProtoPort {
public:
	virtual void attach(ProtoPort upstream)
	{
		throw InternalError("This must be a 'top' port");
	}

	virtual unsigned getUdpPort()                          = 0;

	virtual bool     isFull()                              = 0;

	virtual void connect(const char *ina, unsigned port)   = 0;

	virtual ~IUdpPort() {}

	static UdpPort create(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, unsigned depth = 4);
};

class ITcpPort;
typedef shared_ptr<ITcpPort> TcpPort;

class ITcpPort : public IProtoPort {
public:

	virtual void attach(ProtoPort upstream)
	{
		throw InternalError("This must be a 'top' port");
	}

	virtual unsigned getTcpPort()                 = 0;

	/* in client mode only */
	virtual void connect(const char *ina, unsigned port) = 0;

	virtual ~ITcpPort() {}

	static TcpPort create(const char *ina, unsigned port, unsigned depth = 4, bool isServer = true);
};


class ILoopbackPorts;
typedef shared_ptr<ILoopbackPorts> LoopbackPorts;

class ILoopbackPorts {
public:
	virtual ProtoPort getPortA() = 0;
	virtual ProtoPort getPortB() = 0;

	static LoopbackPorts create(unsigned depth, unsigned lossPercent, unsigned garbl);
};

class ISink: public IProtoPort {
public:
	static ProtoPort create(const char *name, uint64_t sleep_us=0);
};

#endif
