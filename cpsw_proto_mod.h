#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

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
	virtual BufChain pop(struct timespec *timeout) = 0;
	virtual BufChain tryPop()                      = 0;

	virtual ProtoMod pushMod( ProtoMod *m_p )      = 0;

	virtual ProtoMod cloneStack()                  = 0;

	virtual ~IProtoMod() {}
};

typedef queue< IBufChain *, boost::lockfree::fixed_sized< true > > CBufQueueBase;

class CBufQueue : protected CBufQueueBase {
private:
	sem_t rd_sem;
protected:
	BufChain pop(bool wait, struct timespec*);

public:
	CBufQueue(size_type n);

	// waiting to push not implemented ATM
	bool     push(BufChain *owner);

	BufChain pop(struct timespec*);
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

	virtual BufChain pop(struct timespec *timeout)
	{
		return outputQueue_.pop( timeout );
	}

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
