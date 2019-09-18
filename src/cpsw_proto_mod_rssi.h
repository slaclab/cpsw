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

struct CRssiConfigParams {
	static const uint8_t  LD_MAX_UNACKED_SEGS_DFLT = CRssi::LD_MAX_UNACKED_SEGS;
	static const unsigned         QUEUE_DEPTH_DFLT = 0;
	static const uint64_t      REX_TIMEOUT_US_DFLT = CRssi::RETRANSMIT_TIMEO * CRssi::UNIT_US;
	static const uint64_t      CAK_TIMEOUT_US_DFLT = CRssi::CUMLTD_ACK_TIMEO * CRssi::UNIT_US;
	static const uint64_t      NUL_TIMEOUT_US_DFLT = CRssi::NUL_SEGMEN_TIMEO * CRssi::UNIT_US;
	static const uint8_t              REX_MAX_DFLT = CRssi::MAX_RETRANSMIT_N;
	static const uint8_t              CAK_MAX_DFLT = CRssi::MAX_CUMLTD_ACK_N;
	static const unsigned             SGS_MAX_DFLT = 0;

	uint8_t      ldMaxUnackedSegs_;
	unsigned     outQueueDepth_;
	unsigned     inpQueueDepth_;
	uint64_t     rexTimeoutUS_;
	uint64_t     cumAckTimeoutUS_;
	uint64_t     nulTimeoutUS_;
	uint8_t      rexMax_;
	uint8_t      cumAckMax_;
	unsigned     forcedSegsMax_;

	CRssiConfigParams(
		uint8_t      ldMaxUnackedSegs = LD_MAX_UNACKED_SEGS_DFLT,
		unsigned     outQueueDepth    = QUEUE_DEPTH_DFLT,
		unsigned     inpQueueDepth    = QUEUE_DEPTH_DFLT,
		uint64_t     rexTimeoutUS     = REX_TIMEOUT_US_DFLT,
		uint64_t     cumAckTimeoutUS  = CAK_TIMEOUT_US_DFLT,
		uint64_t     nulTimeoutUS     = NUL_TIMEOUT_US_DFLT,
		uint8_t      rexMax           = REX_MAX_DFLT,
		uint8_t      cumAckMax        = CAK_MAX_DFLT,
		unsigned     forcedSegsMax    = SGS_MAX_DFLT
	)
	:
		ldMaxUnackedSegs_( ldMaxUnackedSegs ),
		outQueueDepth_   ( outQueueDepth    ),
		inpQueueDepth_   ( inpQueueDepth    ),
		rexTimeoutUS_    ( rexTimeoutUS     ),
		cumAckTimeoutUS_ ( cumAckTimeoutUS  ),
		nulTimeoutUS_    ( nulTimeoutUS     ),
		rexMax_          ( rexMax           ),
		cumAckMax_       ( cumAckMax        ),
		forcedSegsMax_   ( forcedSegsMax    )
	{}
};

class CProtoModRssi: public CShObj, private CRssiConfigParams, public IPortImpl, public CProtoModBase, public CRssi, public IMTUQuerier {
private:
	ProtoDoor upstream_;
	unsigned  forcedSegsMax_;

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
	  CRssiConfigParams( config ? *config : CRssiConfigParams() ),
	  CRssi(
		false,
		threadPriority,
		this,
		ldMaxUnackedSegs_,
		outQueueDepth_,
		inpQueueDepth_,
		rexTimeoutUS_,
		cumAckTimeoutUS_,
		nulTimeoutUS_,
		rexMax_,
		cumAckMax_
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
