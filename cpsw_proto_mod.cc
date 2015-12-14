#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_proto_mod.h>

#include <errno.h>


CBufQueue::CBufQueue(size_type n)
: queue< IBufChain*, boost::lockfree::fixed_sized< true > >(n)
{
	if ( sem_init( &rd_sem, 0, 0 ) ) {
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
	sem_destroy( &rd_sem );
}

bool CBufQueue::push(BufChain *owner)
{
// keep a ref in case we have to undo
BufChain ref = *owner;

	IBufChain::take_ownership(owner);
	// 1 ref inside the BufChain obj
	// 1 ref in our local var
	// (*owner) has been reset

	if ( bounded_push( ref.get() ) ) {
		if ( sem_post( &rd_sem ) ) {
			throw InternalError("FATAL ERROR -- unable to post semaphore");
			// cannot easily clean up since the item is in the queue already
		}
		return true;
	} else {
		// enqueue failed -- re-transfer smart pointer
		// to owner...
		*owner = ref->yield_ownership();
	}
	return false;
}

BufChain CBufQueue::pop(bool wait, struct timespec *timeout)
{
int sem_stat = wait ?
	                   ( timeout ? sem_timedwait( &rd_sem, timeout )
	                             : sem_wait( &rd_sem ) )
                    : sem_trywait( &rd_sem );

	if ( 0 == sem_stat ) {
		IBufChain *raw_ptr;
		if ( !CBufQueueBase::pop( raw_ptr ) ) {
			throw InternalError("FATAL ERROR -- unable to pop even though we decremented the semaphore?");
		}
		return raw_ptr->yield_ownership();
	}

	switch ( errno ) {
		case EAGAIN:
		case ETIMEDOUT:
			return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
		case EINVAL:
			throw InvalidArgError("invalid timeout arg");
		default:
			break;
	}

	throw IOError("sem__xxwait failed");
}

BufChain CBufQueue::pop(struct timespec *timeout)
{
	return pop( true, timeout );
}

BufChain CBufQueue::tryPop()
{
	return pop(false, 0);
}
