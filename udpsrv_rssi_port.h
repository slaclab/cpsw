#ifndef RSSI_PORT_H
#define RSSI_PORT_H

#include <udpsrv_util.h>
#include <cpsw_rssi.h>

class CRssiPort;
typedef shared_ptr<CRssiPort> RssiPort;

class CRssiPort : public CRssi, public IProtoPort {
private:
	ProtoPort upstream_;
public:
	CRssiPort(bool isServer)
	: CRssi(isServer)
	{
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
