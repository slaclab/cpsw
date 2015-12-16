#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <cpsw_api_user.h>

#include <boost/lockfree/queue.hpp>
#include <boost/weak_ptr.hpp>
#include <semaphore.h>
#include <time.h>

#include <cpsw_buf.h>

using boost::lockfree::queue;
using boost::weak_ptr;

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class IProtoMod {
public:
	static const bool ABS_TIMEOUT = true;
	static const bool REL_TIMEOUT = false;

	virtual BufChain pop(CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                          = 0;

	virtual ProtoMod pushMod( ProtoMod *m_p )          = 0;

	virtual ProtoMod cloneStack()                      = 0;

	virtual ~IProtoMod() {}
};

typedef queue< IBufChain *, boost::lockfree::fixed_sized< true > > CBufQueueBase;

class CBufQueue : protected CBufQueueBase {
private:
	sem_t rd_sem_;
	sem_t wr_sem_;
protected:
	BufChain pop(bool wait, struct timespec * abs_timeout);

public:
	CBufQueue(size_type n);

	bool     push(BufChain *owner);

	BufChain pop(struct timespec *abs_timeout);
	BufChain tryPop();

	~CBufQueue();
};

class CProtoModKey {
private:
	CProtoModKey() {}
	friend class CProtoMod;
};

class CProtoMod : public virtual IProtoMod {
private:
	weak_ptr<CProtoMod> self_;	

	static CProtoModKey getKey() { return CProtoModKey(); }

protected:
	CBufQueue outputQueue_;
	ProtoMod  upstream_;

public:
	CProtoMod(CProtoModKey k, CBufQueueBase::size_type n):outputQueue_(n) {}

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

	virtual ProtoMod getUpstream() { return upstream_ ; }

protected:
	virtual ProtoMod clone();

public:

	virtual const char *getName() const = 0;

	virtual ProtoMod pushMod( ProtoMod *m_p );

	virtual ProtoMod cloneStack();

	virtual ~CProtoMod() {}

	template <typename C, typename A1>
	static shared_ptr<C> create(A1 a1)
	{
	C *obj = new C(getKey(), a1);
	shared_ptr<C> rval(obj);
		obj->self_ = rval;
		return rval;
	}
	template <typename C, typename A1, typename A2>
	static shared_ptr<C> create(A1 a1, A2 a2)
	{
	C *obj = new C(getKey(), a1, a2);
	shared_ptr<C> rval(obj);
		obj->self_ = rval;
		return rval;
	}
	template <typename C, typename A1, typename A2, typename A3>
	static shared_ptr<C> create(A1 a1, A2 a2, A3 a3)
	{
	C *obj = new C(getKey(), a1, a2, a3);
	shared_ptr<C> rval(obj);
		obj->self_ = rval;
		return rval;
	}
	template <typename C, typename A1, typename A2, typename A3, typename A4>
	static shared_ptr<C> create(A1 a1, A2 a2, A3 a3, A4 a4)
	{
	C *obj = new C(getKey(), a1, a2, a3, a4);
	shared_ptr<C> rval(obj);
		obj->self_ = rval;
		return rval;
	}
	template <typename C, typename A1, typename A2, typename A3, typename A4, typename A5>
	static shared_ptr<C> create(A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
	{
	C *obj = new C(getKey(), a1, a2, a3, a4, a5);
	shared_ptr<C> rval(obj);
		obj->self_ = rval;
		return rval;
	}
};

#endif
