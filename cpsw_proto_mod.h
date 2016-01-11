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

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class IProtoMod {
public:
	static const bool ABS_TIMEOUT = true;
	static const bool REL_TIMEOUT = false;

	// returns NULL shared_ptr on timeout; throws on error
	virtual BufChain pop(CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                          = 0;

	// Successfully pushed buffers are unlinked from the chain
	virtual void push(BufChain , CTimeout *, bool abs_timeout) = 0;
	virtual void tryPush(BufChain)                             = 0;

	virtual ProtoMod pushMod( ProtoMod *m_p )          = 0;

	virtual ProtoMod cloneStack()                      = 0;

	virtual ProtoMod getUpstream()                     = 0;

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

class CProtoMod : public virtual IProtoMod, public CShObj {
protected:
	CBufQueue outputQueue_;
	ProtoMod  upstream_;
	CProtoMod(const CProtoMod &orig, Key &k)
	: CShObj(k),
	  outputQueue_(orig.outputQueue_)
	{
	}

public:
	CProtoMod(Key &k, CBufQueueBase::size_type n):CShObj(k), outputQueue_(n) {}

	virtual BufChain pop(CTimeout *timeout, bool abs_timeout)
	{
		if ( ! timeout || timeout->isIndefinite() )
			return outputQueue_.pop( 0 );
		else if ( timeout->isNone() )
			return outputQueue_.tryPop();

		if ( ! abs_timeout ) {
			// arg is rel-timeout
			CTimeout abst( getAbsTimeout( timeout ) );
			return outputQueue_.pop( &abst.tv_ );
		} else {
			return outputQueue_.pop( &timeout->tv_ );
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
		return outputQueue_.tryPop();
	}

	virtual void push(BufChain bc, CTimeout *timeout, bool abs_timeout)
	{
		mustGetUpstream()->push( processOutput( bc ), timeout, abs_timeout );
	}

	virtual void tryPush(BufChain bc)
	{
		mustGetUpstream()->tryPush( processOutput( bc ) );
	}

protected:
	virtual BufChain processOutput(BufChain bc)
	{
		throw InternalError("processOutput() not implemented!\n");
	}

	virtual ProtoMod getUpstream()
	{
		return upstream_;
	}

	virtual ProtoMod mustGetUpstream() 
	{
	ProtoMod rval = getUpstream();
		if ( ! rval )
			throw InternalError("mustGetUpstream() received NIL pointer\n");
		return rval;
	}

public:
	virtual CProtoMod *clone(Key &k) = 0;

	virtual const char *getName() const = 0;

	virtual ProtoMod pushMod( ProtoMod *m_p );

	virtual ProtoMod cloneStack();

	virtual void dumpInfo(FILE *f) {}

	virtual ~CProtoMod() {}
};

#endif
