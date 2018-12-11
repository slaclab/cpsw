 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef RSSI_PORT_H
#define RSSI_PORT_H

#include <cpsw_compat.h>
#include <udpsrv_util.h>
#include <cpsw_rssi.h>
#include <udpsrv_stream_state_monitor.h>

class CRssiPort;
typedef shared_ptr<CRssiPort> RssiPort;

class ConnHandler : public IEventHandler {
private:
	cpsw::atomic<bool> isConnected_;

public:
	ConnHandler()
	: isConnected_(false)
	{
	}

	bool
	isConnected()
	{
		return isConnected_.load();
	}

	virtual void handle(IIntEventSource *eventSource)
	{
		isConnected_.store( eventSource->getEventVal() > 0 );
	}
};

class CRssiPort : public CRssi, public IProtoPort {
private:
	ProtoPort  upstream_;
	ConnHandler hdlr;
public:
	CRssiPort(bool isServer)
	: CRssi(isServer)
	{
		StreamStateMonitor::getTheMonitor()->getEventSet()->add((CConnectionStateChangedEventSource*)this, &hdlr);
	}

	virtual unsigned isConnected()
	{
		return hdlr.isConnected();
	}

	virtual ProtoPort getUpstreamPort()
	{
		return upstream_;
	}

	virtual void attach(ProtoPort upstream)
	{
		upstream_ = upstream;
		CRssi::attach( upstream->getReadEventSource() );
	}

	virtual BufChain pop(const CTimeout *abs_timeout)
	{
		return outQ_->pop(abs_timeout);
	}

	virtual BufChain tryPop()
	{
		return outQ_->tryPop();
	}

	virtual bool push(BufChain bc, const CTimeout *abs_timeout)
	{
		return inpQ_->push(bc, abs_timeout);
	}

	virtual bool tryPushUpstream( BufChain bc )
	{
		return upstream_->tryPush( bc );
	}

	virtual BufChain tryPopUpstream()
	{
		return upstream_->tryPop();
	}

	virtual bool tryPush(BufChain bc)
	{
		return inpQ_->tryPush(bc);
	}

	virtual IEventSource *getReadEventSource()
	{
		return outQ_->getReadEventSource();
	}

	virtual IEventSource *getWriteEventSource()
	{
		return outQ_->getWriteEventSource();
	}

	static RssiPort create(bool isServer)
	{
	CRssiPort *p = new CRssiPort(isServer);
		return RssiPort(p);
	}

	virtual void start()
	{
		threadStart();
	}

	virtual void stop()
	{
		threadStop();
	}

};

#endif
