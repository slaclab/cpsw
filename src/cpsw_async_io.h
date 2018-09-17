#ifndef CPSW_ASYNC_TRANSACTION_H
#define CPSW_ASYNC_TRANSACTION_H

#include <cpsw_api_user.h>
#include <cpsw_buf.h>
#include <cpsw_mutex.h>
#include <cpsw_shared_obj.h>
#include <cpsw_freelist.h>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::make_shared;
using boost::weak_ptr;

class   CAsyncIOTransactionNode;
typedef shared_ptr<CAsyncIOTransactionNode> AsyncIOTransactionNode;

class IAsyncIOTransactionManager;
typedef shared_ptr<IAsyncIOTransactionManager> AsyncIOTransactionManager;

class IAsyncIOTransactionManager {
public:
	typedef uint32_t TID;	

	virtual void post(AsyncIOTransactionNode, TID, AsyncIO callback) = 0;
	virtual int  complete(BufChain, TID)                             = 0;

	virtual ~IAsyncIOTransactionManager() {}

	static AsyncIOTransactionManager create(uint64_t timeoutUs = 500000);
};

// A transaction object
//
class CAsyncIOTransactionNode {
private:
	AsyncIOTransactionNode              next_;
	weak_ptr<CAsyncIOTransactionNode>   prev_;

	IAsyncIOTransactionManager::TID     tid_;
	CTimeout                            timeout_;
	AsyncIO                             aio_;

	CAsyncIOTransactionNode(const CAsyncIOTransactionNode &);
	CAsyncIOTransactionNode & operator=(const CAsyncIOTransactionNode &);

protected:

	void addAfter(AsyncIOTransactionNode node);

	void addBefore(AsyncIOTransactionNode node);

	bool isLonely() const;

	void remove ();

	virtual ~CAsyncIOTransactionNode()
	{
	}
	
public:
	CAsyncIOTransactionNode()
	{
	}

	virtual AsyncIOTransactionNode getSelfAsAsyncIOTransactionNode() = 0;

	// to be overridden by concrete transaction classes
	virtual void complete(BufChain) = 0;

	friend class CAsyncIOTransactionManager;
};

class CAsyncIOTransaction;
typedef shared_ptr<CAsyncIOTransaction> AsyncIOTransaction;

typedef CFreeListNodeKey<CAsyncIOTransaction> CAsyncIOTransactionKey;

class CAsyncIOTransaction : public CFreeListNode<CAsyncIOTransaction>, public CAsyncIOTransactionNode {
public:
	CAsyncIOTransaction( const CAsyncIOTransactionKey &k )
	: CFreeListNode<CAsyncIOTransaction>( k )
	{
	}

	virtual AsyncIOTransactionNode getSelfAsAsyncIOTransactionNode()
	{
		return getSelfAs<AsyncIOTransaction>();
	}
};

template <typename C> class CAsyncIOTransactionPool : public CFreeList<C, CAsyncIOTransaction> {
};

class CAsyncIOCompletion : public IAsyncIO {
private:
	AsyncIO stack_;
public:
	CAsyncIOCompletion(AsyncIO parent);

	virtual void callback (CPSWError *upstreamError);

	virtual void complete() = 0;
};

/* A utility class which can be used if the work for the parent request
 * is parallelized.
 * In that case, the parent callback should only be executed once
 * all of the sub-work is finished.
 * We accomplish this by submitting a shared pointer to a single
 * CAsyncIOParallelCompletion object to all of the sub-work tasks.
 * And we then execute the parent callback from the CAsyncIOParallelCompletion's
 * destructor, thus letting the shared pointer keeping track for us.
 */

class IAsyncIOParallelCompletion;
typedef shared_ptr<IAsyncIOParallelCompletion> AsyncIOParallelCompletion;

class IAsyncIOParallelCompletion : public IAsyncIO {
public:
	// the callback is executed by all the sub-work tasks and merely
	// records either the first or the last error encountered.
	virtual void callback(CPSWError *subWorkError) = 0;

	// once the last reference to this object goes out of scope
	// the parent's callback is executed from this destructor.
	virtual ~IAsyncIOParallelCompletion()          {}

	// record either the last or the first error and pass to the parent callback
	static AsyncIOParallelCompletion create(AsyncIO parent, bool recordLastError = false);
};

class IAsyncIOCompletionWaiter;
typedef shared_ptr<IAsyncIOCompletionWaiter> AsyncIOCompletionWaiter;

// IAsyncIO which synchronizes with the IAsyncIO callback
class IAsyncIOCompletionWaiter : public IAsyncIO {
public:
    virtual void wait()                   = 0;

	virtual ~IAsyncIOCompletionWaiter()   {}

	static AsyncIOCompletionWaiter create();
};

#endif
