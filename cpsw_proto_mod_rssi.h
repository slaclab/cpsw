#ifndef CPSW_PROTO_MOD_RSSI_H
#define CPSW_PROTO_MOD_RSSI_H

#include <cpsw_proto_mod.h>
#include <cpsw_rssi.h>

class CProtoModRssi;
typedef shared_ptr<CProtoModRssi> ProtoModRssi;

class CProtoModRssi: public CShObj, public IPortImpl, public IProtoMod, public CRssi {
private:
	ProtoPort upstream_;

protected:
	virtual int           iMatch(ProtoPortMatchParams *cmp);

public:
	CProtoModRssi(Key &k)
	: CShObj(k),
	  CRssi(false)
	{
	}

	virtual const char *  getName() const;

	virtual BufChain      pop(const CTimeout *timeout, bool abs_timeout);
	virtual BufChain      tryPop();

	virtual bool          push(BufChain, const CTimeout *timeout, bool abs_timeout);
	virtual bool          tryPush(BufChain);

	virtual ProtoMod      getProtoMod();
	virtual ProtoMod      getUpstreamProtoMod();
	virtual ProtoPort     getUpstreamPort();

	virtual void          modStartup();
	virtual void          modShutdown();

	virtual IEventSource *getReadEventSource();
	
	virtual CTimeout      getAbsTimeoutPop(const CTimeout *rel_timeout);
	virtual CTimeout      getAbsTimeoutPush(const CTimeout *rel_timeout);


	virtual void          addAtPort(ProtoMod downstreamMod);
	virtual void          attach(ProtoPort upstream);

	virtual bool          tryPushUpstream(BufChain);
	virtual BufChain      tryPopUpstream();

	virtual bool          pushDown(BufChain, const CTimeout *rel_timeout);

	virtual void          dumpInfo(FILE *);

	virtual ~CProtoModRssi();

	static ProtoModRssi create();
};

#endif
