#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <cpsw_api_user.h>

#include <boost/weak_ptr.hpp>
#include <stdio.h>

#include <cpsw_buf.h>
#include <cpsw_shared_obj.h>
#include <cpsw_event.h>

using boost::weak_ptr;

class IProtoPort;
typedef shared_ptr<IProtoPort> ProtoPort;

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class CPortImpl;
typedef shared_ptr<CPortImpl> ProtoPortImpl;

// find a protocol stack based on parameters
class ProtoPortMatchParams {
public:
	class MatchParam {
	public:
		ProtoPort matchedBy_;
		bool      doMatch_;
		MatchParam(bool doMatch = false)
		: doMatch_(doMatch)
		{
		}
	};
	class MatchParamUnsigned : public MatchParam {
	public:
		unsigned val_;
		MatchParamUnsigned(unsigned val = (unsigned)-1, bool doMatch = false)
		: MatchParam( doMatch ? true : val != (unsigned)-1 ),
		  val_(val)
		{
		}
		MatchParamUnsigned & operator=(unsigned val)
		{
			val_     = val;
			doMatch_ = true;
			return *this;
		}
	};
	MatchParamUnsigned udpDestPort_, srpVersion_, srpVC_;

	int requestedMatches()
	{
	int rval = 0;
		if ( udpDestPort_.doMatch_ )
			rval++;
		if ( srpVersion_.doMatch_ )
			rval++;
		if ( srpVC_.doMatch_ )
			rval++;
		return rval;
	}
};

class IProtoPort {
public:
	static const bool         ABS_TIMEOUT = true;
	static const bool         REL_TIMEOUT = false;

	// returns NULL shared_ptr on timeout; throws on error
	virtual BufChain pop(const CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                          = 0;

	// Successfully pushed buffers are unlinked from the chain
	virtual bool push(BufChain , const CTimeout *, bool abs_timeout) = 0;
	virtual bool tryPush(BufChain)                        = 0;

	virtual ProtoMod  getProtoMod()                       = 0;
	virtual ProtoPort getUpstreamPort()                   = 0;

	virtual void      addAtPort(ProtoMod downstreamMod)   = 0;

	virtual IEventSource *getReadEventSource()            = 0;

	virtual CTimeout  getAbsTimeoutPop (const CTimeout *) = 0;
	virtual CTimeout  getAbsTimeoutPush(const CTimeout *) = 0;

	virtual int       match(ProtoPortMatchParams*)        = 0;
};

class IProtoMod {
public:
	// to be called by the upstream module's addAtPort() method
	// (which is protocol specific)
	virtual void attach(ProtoPort upstream)            = 0;

	virtual ProtoPort getUpstreamPort()                = 0;
	virtual ProtoMod  getUpstreamProtoMod()            = 0;

	virtual void dumpInfo(FILE *)                      = 0;

	virtual void modStartup()                          = 0;
	virtual void modShutdown()                         = 0;

	virtual const char *getName() const                = 0;

	virtual ~IProtoMod() {}
};

class CPortImpl : public IProtoPort {
private:
	weak_ptr< ProtoMod::element_type > downstream_;
	BufQueue                           outputQueue_;

protected:

	CPortImpl(const CPortImpl &orig)
	{
		// would have to set downstream_ to
		// the respective clone and clone
		// the output queue...
		throw InternalError("clone not implemented");
	}

	virtual BufChain processOutput(BufChain bc)
	{
		throw InternalError("processOutput() not implemented!\n");
	}

	virtual BufChain processInput(BufChain bc)
	{
		throw InternalError("processInput() not implemented!\n");
	}

	virtual ProtoPort getSelfAsProtoPort() = 0;

public:
	CPortImpl(unsigned n)
	: outputQueue_( n > 0 ? IBufQueue::create( n ) : BufQueue() )
	{
	}

	virtual IEventSource *getReadEventSource()
	{
		return outputQueue_ ? outputQueue_->getReadEventSource() : NULL;
	}

	virtual ProtoPort mustGetUpstreamPort() 
	{
	ProtoPort rval = getUpstreamPort();
		if ( ! rval )
			throw InternalError("mustGetUpstreamPort() received NIL pointer\n");
		return rval;
	}

	virtual void addAtPort(ProtoMod downstream)
	{
		if ( ! downstream_.expired() )
			throw ConfigurationError("Already have a downstream module");
		downstream_ = downstream;
		downstream->attach( getSelfAsProtoPort() );
	}

	virtual BufChain pop(const CTimeout *timeout, bool abs_timeout)
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

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout)
	{
		if ( outputQueue_ ) {
			if ( !rel_timeout || rel_timeout->isIndefinite() ) {
				return outputQueue_->push( bc, 0 );
			} else if ( rel_timeout->isNone() ) {
				return outputQueue_->tryPush( bc );
			} else {
				CTimeout abst( outputQueue_->getAbsTimeoutPush( rel_timeout ) );
				return outputQueue_->push( bc, &abst );
			}
		} else {
			throw InternalError("cannot push downstream without a queue");
		}
	}
	// getAbsTimeout is not a member of the CTimeout class:
	// the clock to be used is implementation dependent.
	// ProtoMod uses a semaphore which uses CLOCK_REALTIME.
	// The conversion to abs-time should be a member
	// of the same class which uses the clock-dependent
	// resource...

	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout)
	{
		if ( ! outputQueue_ )
			return mustGetUpstreamPort()->getAbsTimeoutPop( rel_timeout );
		return outputQueue_->getAbsTimeoutPop( rel_timeout );
	}

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)
	{
		return mustGetUpstreamPort()->getAbsTimeoutPush( rel_timeout );
	}

	virtual BufChain tryPop()
	{
		if ( ! outputQueue_ ) {
			return processInput( mustGetUpstreamPort()->tryPop() );
		} else {
			return outputQueue_->tryPop();
		}
	}

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
	{
		return mustGetUpstreamPort()->push( processOutput( bc ), timeout, abs_timeout );
	}

	virtual bool tryPush(BufChain bc)
	{
		return mustGetUpstreamPort()->tryPush( processOutput( bc ) );
	}

	virtual ProtoPort getUpstreamPort()
	{
		return getProtoMod()->getUpstreamPort();
	}

	virtual ~CPortImpl()
	{
	}

	// return number of parameters matched by this port
	virtual int iMatch(ProtoPortMatchParams *cmp)
	{
		return 0;
	}

	virtual int match(ProtoPortMatchParams *cmp)
	{
		int rval = iMatch(cmp);

		ProtoPort up( getUpstreamPort() );

		if ( up )
			rval += up->match(cmp);
		return rval;
	}

};

class CProtoModImpl : public IProtoMod {
protected:
	ProtoPort  upstream_;
	CProtoModImpl(const CProtoModImpl &orig)
	{
		// leave upstream_ NULL
	}

public:
	CProtoModImpl()
	{
	}

	// subclasses may want to start threads
	// once the module is attached
	virtual void modStartup()
	{
	}

	virtual void modShutdown()
	{
	}

	virtual void attach(ProtoPort upstream)
	{
		if ( upstream_ )
			throw ConfigurationError("Already have an upstream module");
		upstream_ = upstream;
	}

	virtual ProtoPort getUpstreamPort()
	{
		return upstream_;
	}

	virtual ProtoMod  getUpstreamProtoMod()
	{
		ProtoPort p = getUpstreamPort();
		ProtoMod  rval;
		if ( p )
			rval = p->getProtoMod();
		return rval;
	}

	virtual void dumpInfo(FILE *f)
	{
	}
};

// protocol module with single downstream port
class CProtoMod : public CShObj, public CProtoModImpl, public CPortImpl {

protected:
	CProtoMod(const CProtoMod &orig, Key &k)
	: CShObj(k),
	  CProtoModImpl(orig),
	  CPortImpl(orig)
	{
	}

	virtual ProtoPort getSelfAsProtoPort()
	{
		return getSelfAs< shared_ptr<CProtoMod> >();
	}

public:
	CProtoMod(Key &k, unsigned n)
	: CShObj(k),
	  CPortImpl(n)
	{
	}

	virtual bool pushDown(BufChain bc, const CTimeout *rel_timeout)
	{
		// out of downstream port
		return pushDownstream( bc, rel_timeout );
	}


public:

	virtual ProtoMod getProtoMod()
	{
		return getSelfAs< shared_ptr<CProtoMod> >();
	}

	virtual ~CProtoMod()
	{
	}
};

#endif
