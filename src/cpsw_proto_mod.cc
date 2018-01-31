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
	spinlock_.store ( false, boost::memory_order_release );
    openCount_.store( 0,     boost::memory_order_release);
}

int
IPortImpl::match(ProtoPortMatchParams *cmp)
{
	int rval = iMatch(cmp);

	// since we don't hold on to 'up' there is no
	// need to 'close'...
	ProtoDoor up( getProtoMod()->getUpstreamDoor() );

	if ( up ) {
		rval += up->match(cmp);
	}

	return rval;
}

int
IPortImpl::isOpen() const
{
	return openCount_.load( boost::memory_order_acquire );
}

// Here is some cool smart-pointer wizardry:
//
// We have two interfaces; a basic one and extended
// one which offers more functionality or additional
// state:
//
//    class IOpened;
//
//    class IBase {
//       shared_ptr<IOpened> open()              = 0;       
//       void                someBaseFunction()  = 0;
//    };
//
//    class IOpened : public IBase {
//       void functionOnlyAvailableToOpened()    = 0;
//    };
//
// (We use this for streaming endpoints. If nobody holds
// the interface 'open' then traffic should be discarded
// instead of clogging the queue and eventually stalling 
// RSSI).
//
// How can we have a single class implement both interfaces
// but make the IOpened part only visible to someone
// who 'open()'s the base interface?
//
//    class Implementation : public IOpened {
//    private:
//       weak_ptr<Implementation>    self_;
//       shared_ptr<Implementation>  handle_;
//       int                         openCount_;
//    public:
//       void                  someBaseFunction();              // implementation    
//       void                  functionOnlyAvailableToOpened(); // implementation    
//       shared_ptr<IOpened>   open();
//
//       static shared_ptr<IBase> factory();
//    };
//
// The 'factory' creates an Implementation object, makes a shared pointer
// to the **base** interface only and stores -- as usual -- a weak pointer
// to itself in 'self_'.
//
// The user now has only a pointer to the base interface and cannot use
// 'functionOnlyAvailableToOpened()'.
//
// shared_ptr<IBase>  ptr = Implementation::factory();
//
//       ptr->someBaseFunction();
//
//       ptr->functionOnlyAvailableToOpened(); <++++ compiler error; ptr exposes IBase only
//
// We now let the 'open' method return new shared pointers which
// expose the IOpened interface -- however, since the Implementation
// is really managed by the 'true' shared pointer we must make sure
// the special, 'opened' shared pointer does never destroy anything.
//
// Since the 'opened' pointer is not a true strong reference to the
// entire object we keep such a reference around to make sure
// the Implementation is not destroyed (even if all strong base pointers
// go out of scope):
//
//      First we need a special Deletor() for the 'opened' pointer
//      -- see below...
//
//      class CloseManager {
//        void operator()(Implementation *);
//      };
//
//      shared_ptr<IOpened> open()
//      {
//          // create a shared pointer to the full interface
//          // but use a special deletor:
//      shared_ptr<IOpened>  retVal( this, CloseManager() );
//
//          // if the full interface has not been opened yet
//          if ( 0 == openCount_++ ) {
//              // then make sure we keep a 'true' strong reference
//              // to the Implementation.
//              // Remember: an object can only 'truly' be managed
//              // by a *single* shared pointer. And this is the
//              // shared pointer to the base interface.
//              // The following ensures that 'Implementation' remains
//              // alive even if only shared pointers to the opened
//              // interface are left, i.e., handle the scenario:
//              //
//              //   shared_ptr<IBase>   bas = Implementation::create();
//              //   shared_ptr<IOpened> opn = bas->open();
//              //                       bas.reset();
//              //   at this point the 'handle_' ensures that the
//              //   'Implementation' object stays alive.
//
//              handle_ = shared_ptr<Implementation>( self_ );
//
//              //   if, eventually, 'opn' goes out of scope then the
//              //   CloseManager() will reset 'handle_' and Implementation
//              //   will go away...
//           }
//       return retVal;
//       }
//
//       Remains the 'CloseManager'. This deletor doesn't really 
//       delete anything but just keeps track of the openCount_
//       and resets the handle_ once all 'opened' pointers go away.
//
//       void CloseManager::operator()(Implementation *impl)
//       {
//         if ( 0 == --openCount_ )
//           handle_.reset();
//       }
//
//  Nifty - isn't it ?
//
//  NOTE: the above explanation did not account for multithreading issues
//        (but the true implementation does).

class CloseManager {
public:
	void operator()(IPortImpl *impl);
};

ProtoDoor
IPortImpl::open()
{
ProtoDoor rval( ProtoDoor(this, CloseManager()) );

	while ( spinlock_.exchange( true, boost::memory_order_acquire ) )
		/* spin */;

	if ( 0 == openCount_.fetch_add( 1, boost::memory_order_acq_rel ) )
		forcedSelfReference_ = getSelfAsProtoPort();

	spinlock_.store( false, boost::memory_order_release );

	return rval;
}

ProtoPort
IPortImpl::close()
{
	// return a 'true' reference to the base interface;
	// caller is supposed to reset the 'door' pointer.
	return getSelfAsProtoPort();
}

void
CloseManager::operator()(IPortImpl *impl)
{
// must hold on to a reference until we're done with 'impl'
// Remember: the shared_ptr which caused this deletor to
// be executed does not really manage 'impl'. 'forcedSelfReference_'
// could be the last reference to 'impl'. So we must make
// sure we hold on to it until we're done!
ProtoPort holdOnToAReference = impl->forcedSelfReference_;

	while ( impl->spinlock_.exchange( true, boost::memory_order_acquire ) )
		/* spin */;
	if ( 1 == impl->openCount_.fetch_sub( 1, boost::memory_order_acq_rel ) ) {
		impl->forcedSelfReference_.reset();
	}

	impl->spinlock_.store( false, boost::memory_order_release );

	// drain what might be there -- but stop if someone else opens...
	while ( impl->tryPop() && ! impl->isOpen() )
		;
}

ProtoPort
IPortImpl::getUpstreamPort()
{
	// demote upstream door to a port -- if the caller holds
	// on to the pointer then they should't keep the door open
	ProtoDoor upstrm = getUpstreamDoor();
	return upstrm ? upstrm->close() : ProtoPort();
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

ProtoDoor
CPortImpl::getUpstreamDoor()
{
	return getProtoMod()->getUpstreamDoor();
}

ProtoDoor
CPortImpl::mustGetUpstreamDoor()
{
ProtoDoor rval = getProtoMod()->getUpstreamDoor();
	if ( ! rval )
		throw InternalError("mustGetUpstreamDoor() received NIL pointer\n");
	return rval;
}

void
CPortImpl::addAtPort(ProtoMod downstream)
{
	if ( ! downstream_.expired() )
		throw ConfigurationError("Already have a downstream module");
	downstream_ = downstream;
	downstream->attach( getSelfAsProtoPort()->open() );
}

BufChain
CPortImpl::pop(const CTimeout *timeout, bool abs_timeout)
{
	if ( ! outputQueue_ ) {
		BufChain bc;
		while ( processInput( bc, mustGetUpstreamDoor()->pop(timeout, abs_timeout) ) ) {
				;
		}
		return bc;
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
	if ( ! isOpen() )
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

		if ( rval ) {
			// just went offline - drain 
			while ( ! isOpen() && tryPop() )
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
		return mustGetUpstreamDoor()->getAbsTimeoutPop( rel_timeout );
	return outputQueue_->getAbsTimeoutPop( rel_timeout );
}

CTimeout
CPortImpl::getAbsTimeoutPush(const CTimeout *rel_timeout)
{
	return mustGetUpstreamDoor()->getAbsTimeoutPush( rel_timeout );
}

BufChain
CPortImpl::tryPop()
{
	if ( ! outputQueue_ ) {
		BufChain bc;
		while ( processInput( bc, mustGetUpstreamDoor()->tryPop() ) )
				;
		return bc;
	} else {
		return outputQueue_->tryPop();
	}
}

bool
CPortImpl::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
	if ( ! isOpen() )
		return false;
	while ( bc && bc->getSize() > 0 ) {
		if ( ! mustGetUpstreamDoor()->push( processOutput( &bc ), timeout, abs_timeout ) ) {
			return false;
		}
	}
	return true;
}

bool
CPortImpl::tryPush(BufChain bc)
{
	if ( ! isOpen() )
		return true;
	while ( bc && bc->getSize() > 0 ) {
		if ( ! mustGetUpstreamDoor()->tryPush( processOutput( &bc ) ) ) {
			return false;
		}
	}
	return true;
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
CProtoModImpl::attach(ProtoDoor upstream)
{
	if ( upstream_ )
		throw ConfigurationError("Already have an upstream module");
	upstream_ = upstream;
}

ProtoDoor
CProtoModImpl::getUpstreamDoor()
{
	return upstream_;
}

ProtoMod
CProtoModImpl::getUpstreamProtoMod()
{
// since we don't hold on to 'p' there is no
// need to 'close'...
ProtoDoor p = getUpstreamDoor();
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
