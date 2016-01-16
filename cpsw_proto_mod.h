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

class IProtoPort {
public:
	static const bool ABS_TIMEOUT = true;
	static const bool REL_TIMEOUT = false;

	// returns NULL shared_ptr on timeout; throws on error
	virtual BufChain pop(CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                          = 0;

	// Successfully pushed buffers are unlinked from the chain
	virtual void push(BufChain , CTimeout *, bool abs_timeout) = 0;
	virtual void tryPush(BufChain)                             = 0;

	virtual ProtoMod getProtoMod()                             = 0;
};

class IProtoMod {
public:
	// to be called by the upstream module's addAtPort() method
	// (which is protocol specific)
	virtual void attach(ProtoPort upstream)            = 0;

	virtual ProtoPort getUpstreamPort()                = 0;
	virtual ProtoMod  getUpstreamProtoMod()            = 0;


	virtual void dumpInfo(FILE *)                      = 0;

	virtual ~IProtoMod() {}
};

typedef queue< IBufChain *, boost::lockfree::fixed_sized< true > > CBufQueueBase;

class CBufQueue : protected CBufQueueBase {
private:
	unsigned n_;
	sem_t rd_sem_;
	sem_t wr_sem_;
	CBufQueue & operator=(const CBufQueue &orig) { throw InternalError("Must not assign"); }

protected:
	BufChain pop(bool wait, struct timespec * abs_timeout);

public:
	CBufQueue(size_type n);
	CBufQueue(const CBufQueue &);

	bool     push(BufChain *owner);

	BufChain pop(struct timespec *abs_timeout);
	BufChain tryPop();

	~CBufQueue();
};

class CProtoMod : public IProtoMod, public IProtoPort, public CShObj {
private:
	weak_ptr< ProtoMod::element_type > downstream_;

protected:
	CBufQueue *outputQueue_;
	ProtoPort  upstream_;
	CProtoMod(const CProtoMod &orig, Key &k)
	: CShObj(k),
	  outputQueue_(orig.outputQueue_ ? new CBufQueue( *orig.outputQueue_ ) : NULL)
	{
	}

public:
	CProtoMod(Key &k, CBufQueueBase::size_type n)
	:CShObj(k),
	 outputQueue_( n > 0 ? new CBufQueue( n ) : NULL )
	{
	}

	virtual void attach(ProtoPort upstream)
	{
		if ( upstream_ )
			throw ConfigurationError("Already have an upstream module");
		upstream_ = upstream;
	}

	virtual void addAtPort(ProtoMod downstream)
	{
		if ( ! downstream_.expired() )
			throw ConfigurationError("Already have a downstream module");
		downstream_ = downstream;
		downstream->attach( getSelfAs< shared_ptr<CProtoMod> >() );
	}

	virtual BufChain pop(CTimeout *timeout, bool abs_timeout)
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
				CTimeout abst( getAbsTimeout( timeout ) );
				return outputQueue_->pop( &abst.tv_ );
			} else {
				return outputQueue_->pop( &timeout->tv_ );
			}
		}
	}

	// getAbsTimeout is not a member of the CTimeout class:
	// the clock to be used is implementation dependent.
	// ProtoMod uses a semaphore which uses CLOCK_REALTIME.
	// The conversion to abs-time should be a member
	// of the same class which uses the clock-dependent
	// resource...
	virtual CTimeout getAbsTimeout(CTimeout *rel_timeout);

	virtual BufChain tryPop()
	{
		if ( ! outputQueue_ ) {
			return processInput( mustGetUpstreamPort()->tryPop() );
		} else {
			return outputQueue_->tryPop();
		}
	}

	virtual void push(BufChain bc, CTimeout *timeout, bool abs_timeout)
	{
		mustGetUpstreamPort()->push( processOutput( bc ), timeout, abs_timeout );
	}

	virtual void tryPush(BufChain bc)
	{
		mustGetUpstreamPort()->tryPush( processOutput( bc ) );
	}

protected:
	virtual BufChain processOutput(BufChain bc)
	{
		throw InternalError("processOutput() not implemented!\n");
	}

	virtual BufChain processInput(BufChain bc)
	{
		throw InternalError("processInput() not implemented!\n");
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

	virtual ProtoPort mustGetUpstreamPort() 
	{
	ProtoPort rval = getUpstreamPort();
		if ( ! rval )
			throw InternalError("mustGetUpstreamPort() received NIL pointer\n");
		return rval;
	}

public:
	virtual const char *getName() const = 0;

	virtual void dumpInfo(FILE *f) {}

	virtual ProtoMod getProtoMod()
	{
		return getSelfAs< shared_ptr<CProtoMod> >();
	}

	virtual ~CProtoMod()
	{
		if ( outputQueue_ )
			delete outputQueue_;
	}
};

#endif
