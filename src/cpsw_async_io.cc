
#include <cpsw_api_timeout.h>
#include <cpsw_async_io.h>
#include <cpsw_mutex.h>
#include <cpsw_thread.h>
#include <cpsw_condvar.h>
#include <cpsw_error.h>
#include <cpsw_shared_obj.h>

#include <boost/make_shared.hpp>

using boost::make_shared;

class CListAnchor : public CShObj, public CAsyncIOTransactionNode {
private:
	CListAnchor( const CListAnchor &);
	CListAnchor & operator=(const CListAnchor&);
public:
	CListAnchor( CShObj::Key k )
	: CShObj( k )
	{
	}

	virtual AsyncIOTransactionNode getSelfAsAsyncIOTransactionNode()
	{
		// AsyncIOTransactionNode does not derive from CShObj, hence we cannot
		// use CShObj::getSelfAs...
		throw InternalError("CListAnchor::getSelfAsAsyncIOTransactionNode() should not be called");
	}

	virtual void complete(BufChain bc)
	{
	}
};

// call 'complete' in an exception-safe way
class Completer {
private:
	AsyncIOTransactionNode xact_;
	Completer(const Completer&);
	Completer operator=(const Completer&);
public:
	Completer(AsyncIOTransactionNode xact)
	: xact_(xact)
	{
	}

	void operator()(BufChain bc)
	{
		xact_->complete(bc);
	}

};

/* Assume lists always have a head and a tail node and therefore
 * we never have to check that any of the pointers are NULL.
 */
void CAsyncIOTransactionNode::addAfter(AsyncIOTransactionNode node)
{
AsyncIOTransactionNode me = getSelfAsAsyncIOTransactionNode();
	next_              = node->next_;
	node->next_->prev_ = me;
	node->next_        = me;
	prev_              = node;
}

void CAsyncIOTransactionNode::addBefore(AsyncIOTransactionNode node)
{
AsyncIOTransactionNode me       = getSelfAsAsyncIOTransactionNode();
AsyncIOTransactionNode nodePrev = AsyncIOTransactionNode( node->prev_ );
	prev_           = nodePrev;
	nodePrev->next_ = me;
	node->prev_     = me;
	next_           = node;
}

void CAsyncIOTransactionNode::remove ()
{
AsyncIOTransactionNode prev = AsyncIOTransactionNode( prev_ );
	next_->prev_ = prev;
	prev->next_  = next_;
}


class CAsyncIOTransactionManager : public CRunnable, public IAsyncIOTransactionManager {
private:
	CMtx                     pendingMtx_;  // protect pendingList_
	AsyncIOTransactionNode   pendingListHead_; // list head + tail of pending transactions
	AsyncIOTransactionNode   pendingListTail_; // list head + tail of pending transactions

	CCond                    havePending_; // condvar to signal that there are new transactions
	                                       // wakes up the timeout thread

	CTimeout                 timeout_;

	void addHead_unl(AsyncIOTransactionNode node)
	{
		node->addAfter( pendingListHead_ );
	}

	void addTail_unl(AsyncIOTransactionNode node)
	{
		node->addBefore( pendingListTail_ );
	}

protected:
	virtual void doComplete(AsyncIOTransactionNode xact, BufChain bc = BufChain());

public:

	CAsyncIOTransactionManager(uint64_t timeoutUs);

	bool
	pendingListEmpty()
	{
		return pendingListHead_->next_ == pendingListTail_;
	}


	AsyncIOTransactionNode
	getHead_unl()
	{
		return pendingListEmpty() ?  AsyncIOTransactionNode() : pendingListHead_->next_;
	}

	virtual void
	post(AsyncIOTransactionNode xact, TID tid, AsyncIO callback);

	virtual int
	complete(BufChain bc, TID tid);


	virtual void *
	threadBody();

	virtual ~CAsyncIOTransactionManager();
};

CAsyncIOTransactionManager::CAsyncIOTransactionManager(uint64_t timeoutUs)
: CRunnable("AsyncIOTimeout"),
  pendingListHead_( CShObj::create< shared_ptr<CListAnchor> >() ),
  pendingListTail_( CShObj::create< shared_ptr<CListAnchor> >() ),
  timeout_ (timeoutUs)
{
		pendingListHead_->next_ = pendingListTail_;
		pendingListTail_->prev_ = pendingListHead_;
		threadStart();
}

void
CAsyncIOTransactionManager::post(AsyncIOTransactionNode xact, TID tid, AsyncIO callback)
{
	xact->tid_        = tid;

	clock_gettime( CLOCK_MONOTONIC, &xact->timeout_.tv_ );

	xact->timeout_   += timeout_;
	xact->aio_        = callback;
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
	AsyncIOTransactionNode xact;

	// access of protected 'pendingList'
	{
		CMtx::lg guard( &pendingMtx_ );
		pendingListTail_->tid_ = tid; // sentinel
		for ( xact = pendingListHead_->next_; xact->tid_ != tid; xact = xact->next_ ) {
			/* nothing left to do */
		}
		if ( xact == pendingListTail_ ) {
			/* not found! */
			return -1;
		}
		xact->remove();
	}

	doComplete( xact, bc );

	return 0;
}


void *
CAsyncIOTransactionManager::threadBody()
{
	AsyncIOTransactionNode xact;
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
				xact.reset();

			}
bail:
			pthread_cleanup_pop( 1 ); // unlocks pendingMtx_;
		}

		// pendingMtx_ released at this point

		if ( xact ) {
			doComplete( xact );
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

void
CAsyncIOTransactionManager::doComplete(AsyncIOTransactionNode xact, BufChain bc )
{
Completer cmpl( xact );
AsyncIO   aio;

	xact->aio_.swap( aio );

	try {
		cmpl( bc );
		if ( aio ) {
			TimeoutError err;
			aio->callback( bc ? 0 : &err );
		}
	} catch (CPSWError &err) {
		if ( aio )
			aio->callback( &err );
	}
}

CAsyncIOTransactionManager::~CAsyncIOTransactionManager()
{
AsyncIOTransactionNode xact;

	threadStop();

	// flush remaining transactions
	pendingMtx_.l();
	while ( (xact = getHead_unl()) ) {
		xact->remove();
		pendingMtx_.u();
		doComplete( xact );
		pendingMtx_.l();
	}
	pendingMtx_.u();
}

AsyncIOTransactionManager
IAsyncIOTransactionManager::create(uint64_t timeoutUs)
{
	return make_shared<CAsyncIOTransactionManager>( timeoutUs );
}

CAsyncIOCompletion::CAsyncIOCompletion(AsyncIO parent)
: stack_(parent)
{
}

void
CAsyncIOCompletion::callback (CPSWError *upstreamError)
{
	if ( stack_ ) {
		if ( upstreamError ) {
			stack_->callback( upstreamError );
		} else {
			try {
				complete();
				stack_->callback( 0 );
			} catch ( CPSWError &err ) {
				stack_->callback( &err );
			}
		}
	} else {
		complete();
	}
}


class CAsyncIOParallelCompletion : public IAsyncIOParallelCompletion, public CFreeListNode<CAsyncIOParallelCompletion> {
private:
	AsyncIO      parent_;
	bool         recordLastError_;
	CMtx         mutex_;
	CPSWErrorHdl error_;

	CAsyncIOParallelCompletion(const CAsyncIOParallelCompletion &);
	CAsyncIOParallelCompletion operator=(const CAsyncIOParallelCompletion&);

public:
	CAsyncIOParallelCompletion(
		const CFreeListNodeKey<CAsyncIOParallelCompletion> & key,
		AsyncIO parent,
		bool recordLastError = false
	);

	// the callback is executed by all the sub-work tasks and merely
	// records either the first or the last error encountered.
	virtual void callback(CPSWError *subWorkError);

	// once the last reference to this object goes out of scope
	// the parent's callback is executed from this destructor.
	virtual ~CAsyncIOParallelCompletion();

	static AsyncIOParallelCompletion create(AsyncIO parent, bool recordLastError = false);
};

CAsyncIOParallelCompletion::CAsyncIOParallelCompletion(
	const CFreeListNodeKey<CAsyncIOParallelCompletion> & key,
	AsyncIO parent,
	bool recordLastError
)
: CFreeListNode<CAsyncIOParallelCompletion>( key ),
  parent_          ( parent          ),
  recordLastError_ ( recordLastError )
{
}

void
CAsyncIOParallelCompletion::callback(CPSWError *err)
{
	/* No real work to do; simply record the first (or last) error (in a thread safe way) */
	if ( err &&  (recordLastError_ || ! error_) ) {
		CMtx::lg GUARD( &mutex_ );

		if ( recordLastError_ || ! error_ ) {
			error_ = err->clone();
		}
	}
}

CAsyncIOParallelCompletion::~CAsyncIOParallelCompletion()
{
	parent_->callback( error_ ? error_.get() : 0 );
}

AsyncIOParallelCompletion
CAsyncIOParallelCompletion::create(AsyncIO parent, bool recordLastError)
{
static CFreeList<CAsyncIOParallelCompletion> pool;

shared_ptr<CAsyncIOParallelCompletion> rval = pool.alloc( parent );

	// free-list supports only one initializer...
	if ( rval ) {
		rval->recordLastError_ = recordLastError;
	}

	return rval;
}

AsyncIOParallelCompletion
IAsyncIOParallelCompletion::create(AsyncIO parent, bool recordLastError)
{
	return CAsyncIOParallelCompletion::create(parent, recordLastError);
}

// IAsyncIO which synchronizes with the IAsyncIO callback
class CAsyncIOCompletionWaiter : public CFreeListNode<CAsyncIOCompletionWaiter>, public CMtx, public CCond, public IAsyncIOCompletionWaiter {
private:
    bool         done_;
	CPSWErrorHdl error_;
public:
    CAsyncIOCompletionWaiter(CFreeListNodeKey<CAsyncIOCompletionWaiter> k);

    virtual void callback(CPSWError *err);

    virtual void wait();
};

CAsyncIOCompletionWaiter::CAsyncIOCompletionWaiter(
	CFreeListNodeKey<CAsyncIOCompletionWaiter> key
)
  : CFreeListNode<CAsyncIOCompletionWaiter>( key ),
    done_(false)
{
}

void
CAsyncIOCompletionWaiter::callback(CPSWError *err)
{
int syserr;
	if ( err ) {
		error_ = err->clone();
	}
	{
	CMtx::lg guard( this );
		done_ = true;
		if ( (syserr = pthread_cond_signal( CCond::getp() ) ) ) {
			throw InternalError("CAsyncIOCompletionWaiter: pthread_cond_signal failed", syserr);
		}
	}
}

void
CAsyncIOCompletionWaiter::wait()
{
CMtx::lg guard( this );
int      syserr;
	while ( ! done_ ) {
		if ( (syserr = pthread_cond_wait( CCond::getp(), CMtx::getp() )) ) {
			throw InternalError("CAsyncIOCompletionWaiter: pthread_cond_wait failed", syserr);
		}
	}
	done_ = false; // so they can reuse this object
	if ( error_ ) {
		throw (*error_);
	}
}

AsyncIOCompletionWaiter
IAsyncIOCompletionWaiter::create()
{
static CFreeList<CAsyncIOCompletionWaiter> pool;
	return pool.alloc();
}
