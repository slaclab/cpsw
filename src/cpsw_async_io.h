#ifndef CPSW_ASYNC_TRANSACTION_H
#define CPSW_ASYNC_TRANSACTION_H

#include <cpsw_api_user.h>
#include <cpsw_buf.h>
#include <cpsw_mutex.h>
#include <cpsw_shared_obj.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::make_shared;

class   CAsyncIOTransaction;
typedef CAsyncIOTransaction *AsyncIOTransaction;

class   IAsyncIOTransactionPool;
typedef shared_ptr<IAsyncIOTransactionPool> AsyncIOTransactionPool;

class IAsyncIOTransactionPool {
public:
	virtual void put(AsyncIOTransaction) = 0;

	virtual ~IAsyncIOTransactionPool() {}
};

class CAsyncIOTransactionPoolBase : public IAsyncIOTransactionPool {
private:
	CMtx               freeListMtx_;
	AsyncIOTransaction freeList_;
	
	CAsyncIOTransactionPoolBase( const CAsyncIOTransactionPoolBase & );
	CAsyncIOTransactionPoolBase operator=( const CAsyncIOTransactionPoolBase & );
protected:
	AsyncIOTransaction get();
	void               put(AsyncIOTransaction);
	
	CAsyncIOTransactionPoolBase();
	virtual ~CAsyncIOTransactionPoolBase();
};

class CAsyncIOTransactionKey {
private:
	CAsyncIOTransactionKey(){}
	CAsyncIOTransactionKey(const CAsyncIOTransactionKey &);
	CAsyncIOTransactionKey operator=(const CAsyncIOTransactionKey &);
	template<class EL> friend class CAsyncIOTransactionPool;
	friend class CAsyncIOTransactionManager;
};

template <typename EL> class CAsyncIOTransactionPool : public CAsyncIOTransactionPoolBase, public CShObj {
public:
	CAsyncIOTransactionPool(Key &k)
	: CShObj(k)
	{
	}

	typedef shared_ptr< CAsyncIOTransactionPool<EL> > SP;

	EL *getTransaction()
	{
	EL *xact;
		if ( (xact = static_cast<EL*>( get() )) ) {
			return xact;
		}
		return new EL( getSelfAs< SP >(), CAsyncIOTransactionKey() );
	}

	static SP create()
	{
		return CShObj::create<SP>();
	}
};

class IAsyncIOTransactionManager;
typedef shared_ptr<IAsyncIOTransactionManager> AsyncIOTransactionManager;

class IAsyncIOTransactionManager {
public:
	typedef uint32_t TID;	

	virtual void post(AsyncIOTransaction, TID, AsyncIO callback) = 0;
	virtual int  complete(BufChain, TID)                         = 0;

	virtual ~IAsyncIOTransactionManager() {}

	static AsyncIOTransactionManager create(uint64_t timeoutUs = 500000);
};

// A transaction object
//
class CAsyncIOTransaction {
private:
	AsyncIOTransaction              next_, prev_;
	IAsyncIOTransactionManager::TID tid_;
	CTimeout                        timeout_;
	AsyncIOTransactionPool          pool_;
	AsyncIO                         aio_;

	CAsyncIOTransaction(const CAsyncIOTransaction &);
	CAsyncIOTransaction & operator=(const CAsyncIOTransaction &);

	void addAfter(AsyncIOTransaction node)
	{
		next_              = node->next_;
		node->next_->prev_ = this;
		node->next_        = this;
		prev_              = node;
	}

	void addBefore(AsyncIOTransaction node)
	{
		prev_              = node->prev_;
		node->prev_->next_ = this;
		node->prev_        = this;
		next_              = node;
	}

	bool isLonely() const
	{
		return next_ == this;
	}


	void remove ()
	{
		next_->prev_ = prev_;
		prev_->next_ = next_;
	}

protected:
	CAsyncIOTransaction( AsyncIOTransactionPool pool, const CAsyncIOTransactionKey &key )
	: next_( this ),
	  prev_( this ),
	  pool_(pool)
	{
	}

	virtual ~CAsyncIOTransaction()
	{
	}
	
public:

	// to be overridden by concrete transaction classes
	virtual void complete(BufChain) = 0;

	virtual void free()
	{
		if ( aio_ )
			throw InternalError("Callback never executed");
		pool_->put( this );
	}



	friend class CAsyncIOTransactionManager;
	friend class CAsyncIOTransactionPoolBase;
};

class CAsyncIOCompletion : public IAsyncIO {
private:
	AsyncIO stack_;
public:
	CAsyncIOCompletion(AsyncIO parent);

	virtual void callback (CPSWError *upstreamError);

	virtual void complete() = 0;
};

/* A utility class which can be used of the work for the parent request
 * is parallelized.
 * In that case, the parent callback should only be executed once
 * all of the sub-work is finished.
 * We accomplish this by submitting a shared pointer to a single
 * CAsyncIOParallelCompletion object to all of the sub-work tasks.
 * And we then execute the parent callback from the CAsyncIOParallelCompletion's
 * destructor, this letting the shared pointer keeping track for us.
 */

class CAsyncIOParallelCompletion;
typedef shared_ptr<CAsyncIOParallelCompletion> AsyncIOParallelCompletion;

class CAsyncIOParallelCompletion : public IAsyncIO {
private:
	AsyncIO      parent_;
	bool         recordLastError_;
	CMtx         mutex_;
	CPSWErrorHdl error_;

	CAsyncIOParallelCompletion(AsyncIO parent, bool recordLastError);

	CAsyncIOParallelCompletion(const CAsyncIOParallelCompletion &);
	CAsyncIOParallelCompletion operator=(const CAsyncIOParallelCompletion&);

public:
	// the callback is executed by all the sub-work tasks and merely
	// records either the first or the last error encountered.
	virtual void callback(CPSWError *subWorkError);

	// once the last reference to this object goes out of scope
	// the parent's callback is executed from this destructor.
	virtual ~CAsyncIOParallelCompletion();

	static AsyncIOParallelCompletion create(AsyncIO parent, bool recordLastError = false);
};

#endif
