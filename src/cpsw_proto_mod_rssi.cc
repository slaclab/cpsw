 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_proto_mod_rssi.h>

#include <cpsw_yaml.h>
#include <cpsw_stdio.h>

BufChain
CProtoModRssi::pop(const CTimeout *timeout, bool abs_timeout)
{
	if ( ! timeout || timeout->isIndefinite() ) {
		return outQ_->pop( NULL );
	} else if ( timeout->isNone() ) {
		return outQ_->tryPop();
	} else if ( abs_timeout ) {
		return outQ_->pop( timeout );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPop( timeout ) );
		return outQ_->pop( &abst );
	}	
}

bool
CProtoModRssi::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
	if ( ! isOpen() )
		return false;

	if ( ! timeout || timeout->isIndefinite() ) {
		return inpQ_->push( bc, NULL );
	} else if ( timeout->isNone() ) {
		return inpQ_->tryPush( bc );
	} else if ( abs_timeout ) {
		return inpQ_->push( bc, timeout );
	} else {
		CTimeout abst( inpQ_->getAbsTimeoutPush( timeout ) );
		return inpQ_->push( bc, &abst );
	}	
}

bool
CProtoModRssi::pushDown(BufChain bc, const CTimeout *rel_timeout)
{
bool rval;
	if ( ! isOpen() )
		return true;
	if ( ! rel_timeout || rel_timeout->isIndefinite() ) {
		rval = outQ_->push( bc, NULL );
	} else if ( rel_timeout->isNone() ) {
		rval = outQ_->tryPush( bc );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPush( rel_timeout ) );
		rval = outQ_->push( bc, &abst );
	}	
	if ( rval ) {
		while ( ! isOpen() && tryPop() ) {
			/* was just closed; drain - but stop if someone else reopens... */
			;
		}
	}
	return rval;
}

void
CProtoModRssi::dumpYaml(YAML::Node &node) const
{
int               prio = getPrio();
YAML::Node        parms(YAML::NodeType::Null);
CRssiConfigParams deflts;
const CRssiConfigParams *config = getConfigParams();

	if ( prio != IProtoStackBuilder::DFLT_THREAD_PRIORITY ) {
		writeNode(parms, YAML_KEY_threadPriority, prio);
	}
	if( deflts.ldMaxUnackedSegs_ != config->ldMaxUnackedSegs_ ) {
		writeNode(parms, YAML_KEY_ldMaxUnackedSegs, config->ldMaxUnackedSegs_ );
	}
	if( deflts.outQueueDepth_    != config->outQueueDepth_    ) {
		writeNode(parms, YAML_KEY_outQueueDepth, config->outQueueDepth_ );
	}
	if( deflts.inpQueueDepth_    != config->inpQueueDepth_    ) {
		writeNode(parms, YAML_KEY_inpQueueDepth, config->inpQueueDepth_ );
	}
	if( deflts.rexTimeoutUS_     != config->rexTimeoutUS_     ) {
		writeNode(parms, YAML_KEY_retransmissionTimeoutUS, config->rexTimeoutUS_ );
	}
	if( deflts.cumAckTimeoutUS_  != config->cumAckTimeoutUS_  ) {
		writeNode(parms, YAML_KEY_cumulativeAckTimeoutUS, config->cumAckTimeoutUS_  );
	}
	if( deflts.nulTimeoutUS_     != config->nulTimeoutUS_     ) {
		writeNode(parms, YAML_KEY_nullTimeoutUS, config->nulTimeoutUS_ );
	}
	if( deflts.rexMax_           != config->rexMax_           ) {
		writeNode(parms, YAML_KEY_maxRetransmissions, config->rexMax_ );
	}
	if( deflts.cumAckMax_        != config->cumAckMax_        ) {
		writeNode(parms, YAML_KEY_maxCumulativeAcks, config->cumAckMax_ );
	}
	if( deflts.forcedSegsMax_    != config->forcedSegsMax_    ) {
		writeNode(parms, YAML_KEY_maxSegmentSize, config->forcedSegsMax_ );
	}

	writeNode(node, YAML_KEY_RSSI, parms );
}

BufChain
CProtoModRssi::tryPop()
{
	return outQ_->tryPop();
}

bool
CProtoModRssi::tryPush(BufChain bc)
{
	return inpQ_->tryPush( bc );
}

ProtoMod
CProtoModRssi::getProtoMod()
{
	return getSelfAs<ProtoModRssi>();
}

ProtoMod
CProtoModRssi::getUpstreamProtoMod()
{
	return upstream_ ? upstream_->getProtoMod() : ProtoMod();
}

ProtoDoor
CProtoModRssi::getUpstreamDoor()
{
	return upstream_;
}

IEventSource *
CProtoModRssi::getReadEventSource()
{
	return outQ_->getReadEventSource();
}
	
CTimeout
CProtoModRssi::getAbsTimeoutPop(const CTimeout *rel_timeout)
{
	return outQ_->getAbsTimeoutPop( rel_timeout );
}

CTimeout
CProtoModRssi::getAbsTimeoutPush(const CTimeout *rel_timeout)
{
	return inpQ_->getAbsTimeoutPush( rel_timeout );
}

void
CProtoModRssi::attach(ProtoDoor upstream)
{
IEventSource *src = upstream->getReadEventSource();
	upstream_ = upstream;
	if ( ! src ) {
		char buf[512];	
		::snprintf( buf, sizeof(buf), "CProtoModRssi::attach(): Upstream '%s' has no event source\n", upstream->getProtoMod()->getName() );
		throw InternalError( buf );
	}
	CRssi::attach( src );
}

ProtoPort
CProtoModRssi::getSelfAsProtoPort()
{
	return getSelfAs<ProtoModRssi>();
}

void
CProtoModRssi::addAtPort(ProtoMod downstreamMod)
{
	downstreamMod->attach( getSelfAsProtoPort()->open() );
}

void
CProtoModRssi::dumpInfo(FILE *f)
{
	CRssi::dumpStats( f );
}

const char *
CProtoModRssi::getName() const
{
	return "RSSI";
}

void
CProtoModRssi::modStartup()
{
CIntEventHandler eh;
EventSet         evSet = IEventSet::create();
int              connOpen;

	evSet->add( (CConnectionOpenEventSource*)this, &eh );
	threadStart();

	CTimeout relt( 5000000 /*us*/ );

	do {
		CTimeout abst(evSet->getAbsTimeout( &relt ));
		connOpen = eh.receiveEvent( evSet, true, &abst );
		if ( ! connOpen ) {
			fprintf(CPSW::fErr(),"Warning RSSI connection could not be opened (timeout) -- I'll keep trying\n");
//			throw IOError("RSSI connection could not be opened", ETIMEDOUT);
			break;	
		}
	} while ( ! connOpen );
}

BufChain
CProtoModRssi::tryPopUpstream()
{
	return getUpstreamDoor()->tryPop();
}

int CProtoModRssi::iMatch(ProtoPortMatchParams *cmp)
{
	cmp->haveRssi_.handledBy_ = getProtoMod();
	if ( cmp->haveRssi_.doMatch_ ) {
		cmp->haveRssi_.matchedBy_ = getSelfAs<ProtoModRssi>();
		return 1;
	}
	return 0;
}

CProtoModRssi::~CProtoModRssi()
{
}

bool
CProtoModRssi::tryPushUpstream(BufChain bc)
{
	return getUpstreamDoor()->tryPush( bc );
}


void
CProtoModRssi::modShutdown()
{
	threadStop();
}

ProtoModRssi
CProtoModRssi::create(int threadPriority)
{
	return CShObj::create<ProtoModRssi>( threadPriority );
}

unsigned
CProtoModRssi::getMTU()
{
unsigned upMTU = getUpstreamDoor()->getMTU() - RssiHeader::minHeaderSize();
unsigned myMTU = peerSgsMX_;

	return myMTU < upMTU ? myMTU : upMTU;
}

int
CProtoModRssi::getRxMTU()
{
	return getUpstreamDoor()->getMTU();
}
