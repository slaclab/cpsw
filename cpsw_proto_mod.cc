#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_proto_mod.h>

#include <errno.h>

#define QUEUE_PUSH_RETRY_INTERVAL 1000 //nano-seconds

CBufQueue::CBufQueue(size_type n)
: CBufQueueBase(n),
  n_(n)
{
	if ( sem_init( &rd_sem_, 0, 0 ) ) {
		throw InternalError("Unable to create semaphore");
	}
	if ( sem_init( &wr_sem_, 0, n_ ) ) {
		sem_destroy( &rd_sem_ );
		throw InternalError("Unable to create semaphore");
	}
}

CBufQueue::CBufQueue(const CBufQueue &orig)
: CBufQueueBase(orig.n_),
  n_(orig.n_)
{
	if ( sem_init( &rd_sem_, 0, 0 ) ) {
		throw InternalError("Unable to create semaphore");
	}
	if ( sem_init( &wr_sem_, 0, n_ ) ) {
		sem_destroy( &rd_sem_ );
		throw InternalError("Unable to create semaphore");
	}
}

CBufQueue::~CBufQueue()
{
	// since there are raw pointers stored in the queue
	// we must extract the shared_ptr ownership from all
	// stored elements here.
	while ( tryPop() ) {
		;
	}
	sem_destroy( &rd_sem_ );
	sem_destroy( &wr_sem_ );
}

static int
sem_obtain(sem_t *sema_p, bool wait, const CTimeout *abs_timeout)
{
int sem_stat;

	if ( ! wait || ( abs_timeout && abs_timeout->isNone() ) ) {
		sem_stat = sem_trywait( sema_p );
	} else if ( ! abs_timeout || abs_timeout->isIndefinite() ) {
		sem_stat = sem_wait( sema_p );
	} else {
		sem_stat = sem_timedwait( sema_p, &abs_timeout->tv_ );
	}

	if ( sem_stat ) {
		switch ( errno ) {
			case EAGAIN:
			case ETIMEDOUT:
				break;
			case EINVAL:
				throw InvalidArgError("invalid timeout arg");
			case EINTR:
				throw IntrError("interrupted by signal");
			default:
				throw IOError("sem__xxwait failed", errno);
		}
	}
	return sem_stat;
}


bool CBufQueue::push(BufChain *owner, bool wait, const CTimeout *abs_timeout)
{
// keep a ref in case we have to undo
BufChain ref = *owner;

	// wait for a slot
	if ( sem_obtain( &wr_sem_, wait, abs_timeout ) ) {
		return false;
	}

	IBufChain::take_ownership(owner);
	// 1 ref inside the BufChain obj
	// 1 ref in our local var
	// (*owner) has been reset

	if ( bounded_push( ref.get() ) ) {
		if ( sem_post( &rd_sem_ ) ) {
			throw InternalError("FATAL ERROR -- unable to post reader semaphore");
			// cannot easily clean up since the item is in the queue already
		}
		return true;
	} else {

		sem_post( &wr_sem_ );
		// enqueue failed -- re-transfer smart pointer
		// to owner...
		*owner = ref->yield_ownership();

		throw InternalError("Queue inconsistency???");
	}
	return false;
}

BufChain CBufQueue::pop(bool wait, const CTimeout *abs_timeout)
{
int sem_stat = sem_obtain( &rd_sem_, wait, abs_timeout );

	if ( 0 == sem_stat ) {
		IBufChain *raw_ptr;
		if ( !CBufQueueBase::pop( raw_ptr ) ) {
			throw InternalError("FATAL ERROR -- unable to pop even though we decremented the semaphore?");
		}
		BufChain rval = raw_ptr->yield_ownership();
		if ( sem_post( &wr_sem_ ) ) {
			throw InternalError("FATAL ERROR -- unable to post writer semaphore");
		}
		return rval;
	}

	// timeout (or trywait failed)
	return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
}

CTimeout CBufQueue::getAbsTimeout(const CTimeout *rel_timeout)
{
	struct timespec ts;

	if ( ! rel_timeout || rel_timeout->isIndefinite() )
		return TIMEOUT_INDEFINITE;

	if ( clock_gettime( CLOCK_REALTIME, &ts ) )
		throw InternalError("clock_gettime failed", errno);

	CTimeout rval( ts );

	if ( ! rel_timeout->isNone() ) {
		rval += *rel_timeout;
	}
	return rval;
}
