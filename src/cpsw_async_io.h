#ifndef CPSW_ASYNC_TRANSACTION_H
#define CPSW_ASYNC_TRANSACTION_H

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

	virtual void post(AsyncIOTransaction, TID) = 0;
	virtual int  complete(BufChain, TID)       = 0;

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
		pool_->put( this );
	}



	friend class CAsyncIOTransactionManager;
	friend class CAsyncIOTransactionPoolBase;
};



#endif
