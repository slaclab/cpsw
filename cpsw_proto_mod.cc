#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_proto_mod.h>

#include <errno.h>

#define QUEUE_PUSH_RETRY_INTERVAL 1000 //nano-seconds

CBufQueue::CBufQueue(size_type n)
: queue< IBufChain*, boost::lockfree::fixed_sized< true > >(n),
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
: queue< IBufChain*, boost::lockfree::fixed_sized< true > >(orig.n_),
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

bool CBufQueue::push(BufChain *owner)
{
// keep a ref in case we have to undo
BufChain ref = *owner;

	// wait for a slot
	if ( sem_wait( &wr_sem_ ) ) {
		throw InternalError("FATAL ERROR -- blocking for queue semaphore failed");
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

BufChain CBufQueue::pop(bool wait, const struct timespec *abs_timeout)
{
int sem_stat = wait ?
	                   ( abs_timeout ? sem_timedwait( &rd_sem_, abs_timeout )
	                                 : sem_wait( &rd_sem_ ) )
                    : sem_trywait( &rd_sem_ );

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

	switch ( errno ) {
		case EAGAIN:
		case ETIMEDOUT:
			return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
		case EINVAL:
			throw InvalidArgError("invalid timeout arg");
		case EINTR:
			throw IntrError("interrupted by signal");
		default:
			break;
	}

	throw IOError("sem__xxwait failed");
}

BufChain CBufQueue::pop(const struct timespec *timeout)
{
	return pop( true, timeout );
}

BufChain CBufQueue::tryPop()
{
	return pop(false, 0);
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
