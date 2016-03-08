#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <cpsw_api_user.h>

#include <boost/lockfree/queue.hpp>
#include <boost/weak_ptr.hpp>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>

#include <cpsw_buf.h>
#include <cpsw_shared_obj.h>

using boost::lockfree::queue;
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
	virtual void push(BufChain , const CTimeout *, bool abs_timeout) = 0;
	virtual void tryPush(BufChain)                             = 0;

	virtual ProtoMod  getProtoMod()                    = 0;
	virtual ProtoPort getUpstreamPort()                = 0;

	virtual CTimeout  getAbsTimeoutPop (const CTimeout *) = 0;
	virtual CTimeout  getAbsTimeoutPush(const CTimeout *) = 0;

	virtual int       match(ProtoPortMatchParams*)     = 0;
};

class IProtoMod {
public:
	// to be called by the upstream module's addAtPort() method
	// (which is protocol specific)
	virtual void attach(ProtoPort upstream)            = 0;

	virtual ProtoPort getUpstreamPort()                = 0;
	virtual ProtoMod  getUpstreamProtoMod()            = 0;

	virtual bool pushDown(BufChain, const CTimeout *rel_timeout) = 0;

	virtual void dumpInfo(FILE *)                      = 0;

	virtual ~IProtoMod() {}
};

typedef queue< IBufChain *, boost::lockfree::fixed_sized< true > > CBufQueueBase;

class IBufSync;
typedef shared_ptr<IBufSync> BufSync;

class IBufSync {
public:
	virtual bool getEvent( bool wait, const CTimeout *abs_timeout ) = 0;
	virtual void postEvent()                                        = 0;

	virtual BufSync clone()                                         = 0;

	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout)     = 0;

	static CTimeout clockRealtimeGetAbsTimeout(const CTimeout *rel_timeout);
};


class CSemBufSync : public IBufSync {
private:
	int   ini_;
	sem_t sem_;
public:
	CSemBufSync(int val = 0);
	CSemBufSync(const CSemBufSync &orig);

	virtual bool getEvent( bool wait, const CTimeout *);

	virtual void postEvent();

	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout)
	{
		return clockRealtimeGetAbsTimeout( rel_timeout );
	}

	virtual BufSync clone()
	{
	CSemBufSync *p = new CSemBufSync( *this );
		return BufSync( p );
	}

	virtual ~CSemBufSync();
};

class CBufQueue : protected CBufQueueBase {
private:
	unsigned n_;
	BufSync  rd_sync_;
	BufSync  wr_sync_;
	CBufQueue & operator=(const CBufQueue &orig) { throw InternalError("Must not assign"); }

protected:
	BufChain pop(bool wait, const CTimeout * abs_timeout);
	bool    push(BufChain *owner, bool wait, const CTimeout *abs_timeout);

public:
	CBufQueue(size_type n, BufSync rd_sync = BufSync(), BufSync wr_sync = BufSync());
	CBufQueue(const CBufQueue &);


	BufChain pop(const CTimeout *abs_timeout)
	{
		return pop(true, abs_timeout);
	}

	BufChain tryPop()
	{
		return pop(false, 0);
	}


	bool     push(BufChain *owner, const CTimeout *abs_timeout)
	{
		return push(owner, true, abs_timeout);
	}

	bool     tryPush(BufChain *owner)
	{
		return push(owner, false, 0);
	}

	CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout)
	{
		return rd_sync_->getAbsTimeout( rel_timeout );
	}

	CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)
	{
		return wr_sync_->getAbsTimeout( rel_timeout );
	}

	~CBufQueue();
};

class CPortImpl : public IProtoPort {
private:
	weak_ptr< ProtoMod::element_type > downstream_;
	CBufQueue *outputQueue_;

protected:

	CPortImpl(const CPortImpl &orig)
	: outputQueue_(orig.outputQueue_ ? new CBufQueue( *orig.outputQueue_ ) : NULL)
	{
		// would have to set downstream_ to
		// the respective clone...
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
	CPortImpl(CBufQueueBase::size_type n)
	: outputQueue_( n > 0 ? new CBufQueue( n ) : NULL )
	{
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
			if ( !rel_timeout ) {
				return outputQueue_->push( &bc, 0 );
			} else {
				CTimeout abst( getAbsTimeoutPush( rel_timeout ) );
				return outputQueue_->push( &bc, &abst );
			}
		} else {
			return ProtoMod( downstream_ )->pushDown( bc, rel_timeout );
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
			throw ConfigurationError("Cannot compute timeout w/o output queue");
		return outputQueue_->getAbsTimeoutPop( rel_timeout );
	}

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)
	{
		if ( ! outputQueue_ )
			throw ConfigurationError("Cannot compute timeout w/o output queue");
		return outputQueue_->getAbsTimeoutPush( rel_timeout );
	}


	virtual BufChain tryPop()
	{
		if ( ! outputQueue_ ) {
			return processInput( mustGetUpstreamPort()->tryPop() );
		} else {
			return outputQueue_->tryPop();
		}
	}

	virtual void push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
	{
		mustGetUpstreamPort()->push( processOutput( bc ), timeout, abs_timeout );
	}

	virtual void tryPush(BufChain bc)
	{
		mustGetUpstreamPort()->tryPush( processOutput( bc ) );
	}

	virtual ProtoPort getUpstreamPort()
	{
		return getProtoMod()->getUpstreamPort();
	}

	virtual ~CPortImpl()
	{
		if ( outputQueue_ )
			delete outputQueue_;
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

	virtual void attach(ProtoPort upstream)
	{
		if ( upstream_ )
			throw ConfigurationError("Already have an upstream module");
		upstream_ = upstream;
		modStartup();
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

	virtual void dumpInfo(FILE *f) {}
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
	CProtoMod(Key &k, CBufQueueBase::size_type n)
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
	virtual const char *getName() const = 0;

	virtual ProtoMod getProtoMod()
	{
		return getSelfAs< shared_ptr<CProtoMod> >();
	}

	virtual ~CProtoMod()
	{
	}
};

#endif
