 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_RSSI_H
#define CPSW_PROTO_MOD_RSSI_H

#include <cpsw_proto_mod.h>
#include <cpsw_rssi.h>

class CProtoModRssi;
typedef shared_ptr<CProtoModRssi> ProtoModRssi;

class CProtoModRssi: public CShObj, public IPortImpl, public CProtoModBase, public CRssi, public IMTUQuerier {
private:
	ProtoDoor upstream_;

protected:
	virtual int           iMatch(ProtoPortMatchParams *cmp);

private:

public:
	CProtoModRssi(
		Key                     &k,
		int                      threadPriority,
		const CRssiConfigParams *config = 0
	)
	: CShObj(k),
	  CRssi(
		false,
		threadPriority,
		this,
	    config
	  )
	{
	}

	virtual const char *  getName() const;

	virtual BufChain      pop(const CTimeout *timeout, bool abs_timeout);
	virtual BufChain      tryPop();

	virtual bool          push(BufChain, const CTimeout *timeout, bool abs_timeout);
	virtual bool          tryPush(BufChain);

	virtual ProtoMod      getProtoMod();
	virtual ProtoMod      getUpstreamProtoMod();
	virtual ProtoDoor     getUpstreamDoor();
	virtual ProtoPort     getSelfAsProtoPort();

	virtual unsigned      getMTU();

	virtual void          modStartup();
	virtual void          modShutdown();

	virtual IEventSource *getReadEventSource();

	virtual CTimeout      getAbsTimeoutPop(const CTimeout *rel_timeout);
	virtual CTimeout      getAbsTimeoutPush(const CTimeout *rel_timeout);


	virtual void          addAtPort(ProtoMod downstreamMod);
	virtual void          attach(ProtoDoor upstream);

	virtual bool          tryPushUpstream(BufChain);
	virtual BufChain      tryPopUpstream();

	virtual bool          pushDown(BufChain, const CTimeout *rel_timeout);

	virtual void          dumpInfo(FILE *);

	virtual ~CProtoModRssi();

	virtual void dumpYaml(YAML::Node &) const;

	virtual int  getRxMTU();

	static ProtoModRssi create(int threadPriority);
};

#endif
