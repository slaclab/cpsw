#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <boost/lockfree/queue.hpp>
#include <semaphore.h>
#include <time.h>

#include <cpsw_buf.h>

using boost::lockfree::queue;

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

class CProtoMod {
protected:
	CBufQueue outputQueue;
public:
	CProtoMod(CBufQueueBase::size_type n):outputQueue(n) {}

	virtual BufChain pop(struct timespec *timeout)
	{
		return outputQueue.pop( timeout );
	}

	virtual BufChain tryPop()
	{
		return outputQueue.tryPop();
	}

	virtual ~CProtoMod() {}
};

#endif
