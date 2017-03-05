
#include <cpsw_api_timeout.h>
#include <cpsw_async_io.h>
#include <cpsw_mutex.h>
#include <cpsw_thread.h>
#include <cpsw_condvar.h>

#include <boost/make_shared.hpp>

using boost::make_shared;

// Helper class -- a transaction object
//
// The manager will keep a linked list of pending transactions
//
class CTransaction {
private:
	class CTransaction             *next_, *prev_;
	IAsyncIOTransactionManager::TID tid_;
	CTimeout                        timeout_;
	IAsyncIOCallback               *completion_;

	void addAfter(CTransaction *node)
	{
		next_              = node->next_;
		node->next_->prev_ = this;
		node->next_        = this;
		prev_              = node;
	}

	void addBefore(CTransaction *node)
	{
		prev_              = node->prev_;
		node->prev_->next_ = this;
		node->prev_        = this;
		next_              = node;
	}


	void remove ()
	{
		next_->prev_ = prev_;
		prev_->next_ = next_;
	}

	friend class CAsyncIOTransactionManager;
};

static void mutex_unlock_wrapper(void *m)
{
int err;
	if ( (err = pthread_mutex_unlock( (pthread_mutex_t*) m )) ) {
		throw InternalError("CAsyncIOTransactionManager -- unable to unlock mutex", err);
	}
}

class CAsyncIOTransactionManager : public CRunnable, public IAsyncIOTransactionManager {
private:
	CMtx          pendingMtx_;  // protect pendingList_
	CTransaction  pendingList_; // list head + tail of pending transactions

	CCond         havePending_; // condvar to signal that there are new transactions
	                            // wakes up the timeout thread

	CMtx          freeMtx_;     // protect free-list
	CTransaction *freeList_;    // free-list for transactions

	CTimeout      timeout_;

	void addHead_unl(CTransaction *node)
	{
		node->addAfter( &pendingList_ );
	}

	void addTail_unl(CTransaction *node)
	{
		node->addBefore( &pendingList_ );
	}

	CTransaction *getXact()
	{
	CMtx::lg guard( &freeMtx_ );
	CTransaction *xact;
		if ( (xact = freeList_) )
			freeList_ = xact->next_;
		return xact;
	}

	void freeXact(CTransaction *xact)
	{
	CMtx::lg guard( &freeMtx_ );
		xact->next_ = freeList_;
		freeList_   = xact;
	}

public:

	CAsyncIOTransactionManager(uint64_t timeoutUs);

	bool
	pendingListEmpty()
	{
		return pendingList_.next_ == &pendingList_;
	}


	CTransaction *
	getHead_unl()
	{
		return pendingListEmpty() ?  0 : pendingList_.next_;
	}

	virtual void
	post(IAsyncIOCallback *cb, TID tid);

	virtual IAsyncIOCallback *
	cancel(TID tid);


	virtual void *
	threadBody();

	virtual ~CAsyncIOTransactionManager();
};

CAsyncIOTransactionManager::CAsyncIOTransactionManager(uint64_t timeoutUs)
: CRunnable("AsyncIOTimeout"),
  freeList_(0),
  timeout_ (timeoutUs)
{
		pendingList_.next_ = &pendingList_;
		pendingList_.prev_ = &pendingList_;
		threadStart();
}

void
CAsyncIOTransactionManager::post(IAsyncIOCallback *cb, TID tid)
{
	CTransaction *xact = getXact();
	if ( ! xact ) {
		xact = new CTransaction();
	}
	xact->tid_        = tid;
	xact->completion_ = cb;

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

IAsyncIOCallback *
CAsyncIOTransactionManager::cancel(TID tid)
{
	CTransaction     *xact;
	IAsyncIOCallback *rval;

	// access of protected 'pendingList'
	{
		CMtx::lg guard( &pendingMtx_ );
		pendingList_.tid_ = tid; // sentinel
		for ( xact = pendingList_.next_; xact->tid_ != tid; xact = xact->next_ ) {
			/* nothing left to do */
		}
		if ( xact == &pendingList_ ) {
			/* not found! */
			return 0;
		}
		xact->remove();
	}

	rval = xact->completion_;

	freeXact( xact );

	return rval;
}


void *
CAsyncIOTransactionManager::threadBody()
{
	CTransaction *xact;
	CTimeout      now;
	CTimeout      remaining;
	bool          err = false;
	while ( 1 ) {
		{
			pendingMtx_.l();
			pthread_cleanup_push( mutex_unlock_wrapper, (void*)pendingMtx_.getp() );

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
			xact->completion_->complete(BufChain());

			freeXact( xact );
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
CTransaction *xact;

	threadStop();

	// flush remaining transactions
	pendingMtx_.l();
	while ( (xact = getHead_unl()) ) {
		xact->remove();
		pendingMtx_.u();
		xact->completion_->complete( BufChain() );

		delete xact;

		pendingMtx_.l();
	}
	pendingMtx_.u();

	while ( (xact = getXact()) )
		delete xact;

}

AsyncIOTransactionManager
IAsyncIOTransactionManager::create(uint64_t timeoutUs)
{
	return make_shared<CAsyncIOTransactionManager>( timeoutUs );
}
