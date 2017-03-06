
#include <cpsw_api_timeout.h>
#include <cpsw_async_io.h>
#include <cpsw_mutex.h>
#include <cpsw_thread.h>
#include <cpsw_condvar.h>

#include <boost/make_shared.hpp>

using boost::make_shared;

class CListHead : public CAsyncIOTransaction {
public:
	CListHead(const CAsyncIOTransactionKey &k)
	: CAsyncIOTransaction( AsyncIOTransactionPool(), k )
	{
	}

	virtual void free()                { }
	virtual void complete(BufChain bc) { }
};

// call 'complete' in an exception-safe way
class Completer {
private:
	AsyncIOTransaction xact_;
	Completer(const Completer&);
	Completer operator=(const Completer&);
public:
	Completer(AsyncIOTransaction xact)
	: xact_(xact)
	{
	}

	void operator()(BufChain bc)
	{
		xact_->complete(bc);
	}

	~Completer()
	{
		xact_->free();
	}
};


class CAsyncIOTransactionManager : public CRunnable, public IAsyncIOTransactionManager {
private:
	CMtx                 pendingMtx_;  // protect pendingList_
	CListHead            pendingList_; // list head + tail of pending transactions

	CCond                havePending_; // condvar to signal that there are new transactions
	                            // wakes up the timeout thread

	CTimeout             timeout_;

	void addHead_unl(AsyncIOTransaction node)
	{
		node->addAfter( &pendingList_ );
	}

	void addTail_unl(AsyncIOTransaction node)
	{
		node->addBefore( &pendingList_ );
	}

public:

	CAsyncIOTransactionManager(uint64_t timeoutUs);

	bool
	pendingListEmpty()
	{
		return pendingList_.isLonely();
	}


	AsyncIOTransaction 
	getHead_unl()
	{
		return pendingListEmpty() ?  0 : pendingList_.next_;
	}

	virtual void
	post(AsyncIOTransaction xact, TID tid);

	virtual int
	complete(BufChain bc, TID tid);


	virtual void *
	threadBody();

	virtual ~CAsyncIOTransactionManager();
};

CAsyncIOTransactionManager::CAsyncIOTransactionManager(uint64_t timeoutUs)
: CRunnable("AsyncIOTimeout"),
  pendingList_( CAsyncIOTransactionKey() ),
  timeout_ (timeoutUs)
{
		pendingList_.next_ = &pendingList_;
		pendingList_.prev_ = &pendingList_;
		threadStart();
}

void
CAsyncIOTransactionManager::post(AsyncIOTransaction xact, TID tid)
{
	xact->tid_        = tid;

	clock_gettime( CLOCK_MONOTONIC, &xact->timeout_.tv_ );

	xact->timeout_   += timeout_;
	{
		CMtx::lg guard( &pendingMtx_ );
		bool     wakeup = pendingListEmpty();

		addTail_unl( xact );

		if ( wakeup ) {
			if ( pthread_cond_signal( havePending_.getp() ) )
				CondSignalFailed();
		}
	}
}

int
CAsyncIOTransactionManager::complete(BufChain bc, TID tid)
{
	AsyncIOTransaction xact;

	// access of protected 'pendingList'
	{
		CMtx::lg guard( &pendingMtx_ );
		pendingList_.tid_ = tid; // sentinel
		for ( xact = pendingList_.next_; xact->tid_ != tid; xact = xact->next_ ) {
			/* nothing left to do */
		}
		if ( xact == &pendingList_ ) {
			/* not found! */
			return -1;
		}
		xact->remove();
	}

	(Completer (xact))(bc);

	return 0;
}


void *
CAsyncIOTransactionManager::threadBody()
{
	AsyncIOTransaction xact;
	CTimeout      now;
	CTimeout      remaining;
	bool          err = false;
	while ( 1 ) {
		{
			pendingMtx_.l();
			pthread_cleanup_push( CCond::pthread_mutex_unlock_wrapper, (void*)pendingMtx_.getp() );

			while ( ! (xact = getHead_unl()) ) {
				if ( pthread_cond_wait( havePending_.getp(), pendingMtx_.getp() ) ) {
					/* POSIX forbids us to throw an exception here (prematurely
					 * leaving a 'pthread_cleanup_push/pop' bracketed block :-(
					 */
					err = true;
					goto bail;
				}
			}
			clock_gettime( CLOCK_MONOTONIC, &now.tv_ );
			if ( xact->timeout_ < now ) {
				/* Timeout expired */
				xact->remove();

			} else {
				/* wait for the next timeout */
				remaining = xact->timeout_;
				xact      = 0;

			}
bail:
			pthread_cleanup_pop( 1 ); // unlocks pendingMtx_;
		}

		// pendingMtx_ released at this point

		if ( xact ) {
			(Completer(xact))( BufChain() );
		} else {
			// if pthread_cond_wait incurred an error then 'xact == 0' since
			// otherwise we wouldn't have waited in the first place
			if ( err )
				throw CondWaitFailed(); // !!!*** deferred error reporting ***!!!

			clock_nanosleep( CLOCK_MONOTONIC, TIMER_ABSTIME, &remaining.tv_, 0 );
		}
	}
	return 0;
}

CAsyncIOTransactionManager::~CAsyncIOTransactionManager()
{
AsyncIOTransaction xact;

	threadStop();

	// flush remaining transactions
	pendingMtx_.l();
	while ( (xact = getHead_unl()) ) {
		xact->remove();
		pendingMtx_.u();
		(Completer(xact))( BufChain() );

		pendingMtx_.l();
	}
	pendingMtx_.u();
}

AsyncIOTransactionManager
IAsyncIOTransactionManager::create(uint64_t timeoutUs)
{
	return make_shared<CAsyncIOTransactionManager>( timeoutUs );
}

CAsyncIOTransactionPoolBase::CAsyncIOTransactionPoolBase()
: freeList_(0)
{
}

AsyncIOTransaction
CAsyncIOTransactionPoolBase::get()
{
CMtx::lg           guard( &freeListMtx_ );
AsyncIOTransaction xact;
	if ( (xact = freeList_) ) {
		freeList_ = freeList_->next_;
	}
	return xact;
}

void
CAsyncIOTransactionPoolBase::put(AsyncIOTransaction xact)
{
CMtx::lg guard( &freeListMtx_ );
	xact->next_ = freeList_;
	freeList_   = xact;
}


CAsyncIOTransactionPoolBase::~CAsyncIOTransactionPoolBase()
{
CMtx::lg           guard( &freeListMtx_ );
AsyncIOTransaction xact;
	while ( (xact = freeList_) ) {
		freeList_ = freeList_->next_;
		delete xact;
	}
}



