#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_proto_mod.h>

#include <errno.h>

#include <boost/make_shared.hpp>

using boost::make_shared;

#define QUEUE_PUSH_RETRY_INTERVAL 1000 //nano-seconds

CBufQueue::CBufQueue(size_type n)
: CBufQueueBase(n),
  n_(n)
{
	rd_sync_ = make_shared<CSemBufSync>( 0  );
	wr_sync_ = make_shared<CSemBufSync>( n_ );
}

CBufQueue::CBufQueue(const CBufQueue &orig)
: CBufQueueBase(orig.n_),
  n_(orig.n_)
{
	rd_sync_ = make_shared<CSemBufSync>( 0  );
	wr_sync_ = make_shared<CSemBufSync>( n_ );
}

CBufQueue::~CBufQueue()
{
	// since there are raw pointers stored in the queue
	// we must extract the shared_ptr ownership from all
	// stored elements here.
	while ( tryPop() ) {
		;
	}
}

bool CBufQueue::push(BufChain *owner, bool wait, const CTimeout *abs_timeout)
{
// keep a ref in case we have to undo
BufChain ref = *owner;

	// wait for a slot
	if ( ! wr_sync_->getEvent( wait, abs_timeout ) ) {
		return false;
	}

	IBufChain::take_ownership(owner);
	// 1 ref inside the BufChain obj
	// 1 ref in our local var
	// (*owner) has been reset

	if ( bounded_push( ref.get() ) ) {
		rd_sync_->postEvent();
		return true;
	} else {

		wr_sync_->postEvent();

		// enqueue failed -- re-transfer smart pointer
		// to owner...
		*owner = ref->yield_ownership();

		throw InternalError("Queue inconsistency???");
	}
	return false;
}

BufChain CBufQueue::pop(bool wait, const CTimeout *abs_timeout)
{

	if ( rd_sync_->getEvent(wait, abs_timeout) ) {
		IBufChain *raw_ptr;
		if ( !CBufQueueBase::pop( raw_ptr ) ) {
			throw InternalError("FATAL ERROR -- unable to pop even though we decremented the semaphore?");
		}
		BufChain rval = raw_ptr->yield_ownership();
		wr_sync_->postEvent();
		return rval;
	}

	// timeout (or trywait failed)
	return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
}

CTimeout IBufSync::clockRealtimeGetAbsTimeout(const CTimeout *rel_timeout)
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


CSemBufSync::CSemBufSync(int val)
{
	if ( sem_init( &sem_, 0, val ) ) {
		throw InternalError("Unable to create semaphore", errno);
	}
}

void CSemBufSync::postEvent()
{
	if ( sem_post( &sem_ ) )
		throw InternalError("Unable to post semaphore", errno);
}

CSemBufSync::~CSemBufSync()
{
	sem_destroy( &sem_ );
}


bool CSemBufSync::getEvent(bool wait, const CTimeout *abs_timeout)
{
int sem_stat;

	if ( ! wait || ( abs_timeout && abs_timeout->isNone() ) ) {
		sem_stat = sem_trywait( &sem_ );
	} else if ( ! abs_timeout || abs_timeout->isIndefinite() ) {
		sem_stat = sem_wait( &sem_ );
	} else {
		sem_stat = sem_timedwait( &sem_, &abs_timeout->tv_ );
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
	return 0 == sem_stat;
}




