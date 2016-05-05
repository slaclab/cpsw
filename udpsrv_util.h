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

	virtual ProtoPort     getUpstreamPort()                = 0;

	virtual void attach(ProtoPort upstream)                = 0;

	virtual bool push(BufChain, const CTimeout *)          = 0;
	virtual bool tryPush(BufChain)                         = 0;

	virtual ~IProtoPort() {}

	virtual void start()                                   = 0;
	virtual void stop()                                    = 0;

	static ProtoPort create(bool isServer_);
};

class IUdpPort;
typedef shared_ptr<IUdpPort> UdpPort;

class IUdpPort : public IProtoPort {
public:
	virtual BufChain pop(const CTimeout *)       = 0;
	virtual BufChain tryPop()                    = 0;

	virtual void attach(ProtoPort upstream)
	{
		throw InternalError("This must be a 'top' port");
	}

	virtual bool push(BufChain, const CTimeout *) = 0;
	virtual bool tryPush(BufChain)                = 0;

	virtual unsigned isConnected()                = 0;

	virtual ~IUdpPort() {}

	static UdpPort create(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler);
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
