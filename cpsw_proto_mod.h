#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <boost/lockfree/queue.hpp>
#include <semaphore.h>
#include <time.h>

#include <cpsw_buf.h>

using boost::lockfree::queue;

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class IProtoMod {
public:
	virtual BufChain pop(struct timespec *timeout) = 0;
	virtual BufChain tryPop()                      = 0;

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

class CProtoMod : public virtual IProtoMod {
protected:
	CBufQueue outputQueue_;
	ProtoMod  upstream_;
public:
	CProtoMod(CBufQueueBase::size_type n):outputQueue_(n) {}

	virtual BufChain pop(struct timespec *timeout)
	{
		return outputQueue_.pop( timeout );
	}

	virtual BufChain tryPop()
	{
		return outputQueue_.tryPop();
	}

	virtual ~CProtoMod() {}
};

#endif
