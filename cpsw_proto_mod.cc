 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.


#include <cpsw_proto_mod.h>

IPortImpl::IPortImpl()
{
	offline_.store( true, boost::memory_order_release );
}

int
IPortImpl::match(ProtoPortMatchParams *cmp)
{
	int rval = iMatch(cmp);

	ProtoPort up( getUpstreamPort() );

	if ( up ) {
		rval += up->match(cmp);
	}

	return rval;
}

bool
IPortImpl::isOffline() const
{
	return offline_.load( boost::memory_order_acquire );
}

void
IPortImpl::setOffline(bool offline)
{
	offline_.store( offline, boost::memory_order_release );
	if ( offline ) {
		// drain what might be there
		while ( tryPop() )
			;
	}
}

CPortImpl::CPortImpl(unsigned n)
: outputQueue_( n > 0 ? IBufQueue::create( n ) : BufQueue() ),
  depth_(n)
{
}

unsigned
CPortImpl::getQueueDepth() const
{
	return depth_;
}

IEventSource *
CPortImpl::getReadEventSource()
{
	return outputQueue_ ? outputQueue_->getReadEventSource() : NULL;
}

ProtoPort
CPortImpl::mustGetUpstreamPort()
{
ProtoPort rval = getUpstreamPort();
	if ( ! rval )
		throw InternalError("mustGetUpstreamPort() received NIL pointer\n");
	return rval;
}

void
CPortImpl::addAtPort(ProtoMod downstream)
{
	if ( ! downstream_.expired() )
		throw ConfigurationError("Already have a downstream module");
	downstream_ = downstream;
	downstream->attach( getSelfAsProtoPort() );
	setOffline( false );
}

BufChain
CPortImpl::pop(const CTimeout *timeout, bool abs_timeout)
{
	if ( ! outputQueue_ ) {
		return processInput( mustGetUpstreamPort()->pop(timeout, abs_timeout) );
	} else {
		if ( ! timeout || timeout->isIndefinite() )
			return outputQueue_->pop( 0 );
		else if ( timeout->isNone() )
			return outputQueue_->tryPop();

		if ( ! abs_timeout ) {
			// arg is rel-timeout
			CTimeout abst( getAbsTimeoutPop( timeout ) );
			return outputQueue_->pop( &abst );
		} else {
			return outputQueue_->pop( timeout );
		}
	}
}

bool
CPortImpl::pushDownstream(BufChain bc, const CTimeout *rel_timeout)
{
	if ( isOffline() )
		return true;

	if ( outputQueue_ ) {
		bool rval;
		if ( !rel_timeout || rel_timeout->isIndefinite() ) {
			rval = outputQueue_->push( bc, 0 );
		} else if ( rel_timeout->isNone() ) {
			rval = outputQueue_->tryPush( bc );
		} else {
			CTimeout abst( outputQueue_->getAbsTimeoutPush( rel_timeout ) );
			rval = outputQueue_->push( bc, &abst );
		}

		if ( rval && isOffline() ) {
			// just went offline - drain 
			while ( tryPop() )
				;
		}

		return rval;
	} else {
		throw InternalError("cannot push downstream without a queue");
	}
}

CTimeout
CPortImpl::getAbsTimeoutPop(const CTimeout *rel_timeout)
{
	if ( ! outputQueue_ )
		return mustGetUpstreamPort()->getAbsTimeoutPop( rel_timeout );
	return outputQueue_->getAbsTimeoutPop( rel_timeout );
}

CTimeout
CPortImpl::getAbsTimeoutPush(const CTimeout *rel_timeout)
{
	return mustGetUpstreamPort()->getAbsTimeoutPush( rel_timeout );
}

BufChain
CPortImpl::tryPop()
{
	if ( ! outputQueue_ ) {
		return processInput( mustGetUpstreamPort()->tryPop() );
	} else {
		return outputQueue_->tryPop();
	}
}

bool
CPortImpl::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
	if ( isOffline() )
		return false;
	return mustGetUpstreamPort()->push( processOutput( bc ), timeout, abs_timeout );
}

bool
CPortImpl::tryPush(BufChain bc)
{
	if ( isOffline() )
		return true;
	return mustGetUpstreamPort()->tryPush( processOutput( bc ) );
}

ProtoPort
CPortImpl::getUpstreamPort()
{
	return getProtoMod()->getUpstreamPort();
}

CProtoModImpl::CProtoModImpl(const CProtoModImpl &orig)
{
	// leave upstream_ NULL
}

CProtoModImpl::CProtoModImpl()
{
}

void
CProtoModImpl::modStartup()
{
}

void
CProtoModImpl::modShutdown()
{
}

void
CProtoModImpl::attach(ProtoPort upstream)
{
	if ( upstream_ )
		throw ConfigurationError("Already have an upstream module");
	upstream_ = upstream;
}

ProtoPort
CProtoModImpl::getUpstreamPort()
{
	return upstream_;
}

ProtoMod
CProtoModImpl::getUpstreamProtoMod()
{
ProtoPort p = getUpstreamPort();
ProtoMod  rval;
	if ( p )
		rval = p->getProtoMod();
	return rval;
}

void
CProtoModImpl::dumpInfo(FILE *f)
{
}

CProtoMod::CProtoMod(const CProtoMod &orig, Key &k)
: CShObj(k),
  CProtoModImpl(orig),
  CPortImpl(orig)
{
}

ProtoPort
CProtoMod::getSelfAsProtoPort()
{
	return getSelfAs< shared_ptr<CProtoMod> >();
}

CProtoMod::CProtoMod(Key &k, unsigned n)
: CShObj(k),
  CPortImpl(n)
{
}

bool
CProtoMod::pushDown(BufChain bc, const CTimeout *rel_timeout)
{
	// out of downstream port
	return pushDownstream( bc, rel_timeout );
}

ProtoMod
CProtoMod::getProtoMod()
{
	return getSelfAs< shared_ptr<CProtoMod> >();
}
