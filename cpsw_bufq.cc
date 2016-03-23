#include <cpsw_error.h>
#include <cpsw_buf.h>
#include <cpsw_event.h>

#include <errno.h>

#include <boost/lockfree/queue.hpp>
#include <boost/make_shared.hpp>
#include <boost/atomic.hpp>
#include <semaphore.h>

using boost::make_shared;
using boost::lockfree::queue;
using boost::atomic;

typedef queue< IBufChain *, boost::lockfree::fixed_sized< true > > CBufQueueBase;

class IBufSync;
typedef shared_ptr<IBufSync> BufSync;

class IBufSync {
public:
	virtual bool getSlot( bool wait, const CTimeout *abs_timeout )  = 0;
	virtual void putSlot()                                          = 0;

	virtual unsigned getAvailSlots()                                = 0;

	virtual void getAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout) = 0;

	virtual IEventSource *getEventSource()                          = 0;

	static void clockRealtimeGetAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout);
};

class CEventBufSync : public IBufSync, public IIntEventSource, public IEventHandler {
private:
	unsigned    inivl_;
	atomic<int> slots_;
	EventSet    evSet_;
	static const int WATERMARK = 0;
public:
	CEventBufSync(unsigned initialSlots)
	: inivl_(initialSlots),
	  slots_(initialSlots),
	  evSet_(IEventSet::create())
	{
		evSet_->add( /*(IEventSource*)*/ this, /* (IEventHandler*) */ this );
	}

	virtual bool getSlot( bool wait, const CTimeout *abs_timeout );
	virtual void putSlot();

	virtual void getAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout)
	{
		if ( ! evSet_ )
			throw InternalError("No event set");
		evSet_->getAbsTimeout(abs_timeout, rel_timeout);
	}

	virtual unsigned getAvailSlots()
	{
	int v = slots_.load( memory_order_acquire );
		return v <= WATERMARK ? 0 : (unsigned)(v - WATERMARK);
	}

	virtual IEventSource *getEventSource()
	{
		return this;
	}

	// IEventHandler must implement 'handler'
	virtual void handle(IIntEventSource *src)
	{
		// no-op; we call processEvent() and do work around it
	}

	// IIntEventSource must implement 'checkForEvent()'
	virtual bool checkForEvent()
	{
	unsigned avail = getAvailSlots();
		if ( avail > WATERMARK ) {
			setEventVal( avail ); // note; argument must be nonzero!
			return true;
		}
		return false;
	}

	virtual ~CEventBufSync()
	{
		evSet_->del( (IEventSource*)this );
	}
};

bool CEventBufSync::getSlot(bool wait, const CTimeout *abs_timeout)
{
	if ( wait && abs_timeout ) {
		if ( abs_timeout->isNone() ) {
			wait = false;
		} else if ( abs_timeout->isIndefinite() ) {
			abs_timeout = NULL;
		}
	}
	// If 'wait' is 'true' then this is a blocking call. For this
	// to work we assume that the user has not substituted the
	// event set to which we subscribed during creation (if they
	// have they are supposed to use 'processEvent' and use non-blocking
	// 'getSlot()'!
	do {
		if ( WATERMARK < slots_.fetch_sub(1, memory_order_acquire) ) {
			return true;
		}
		// there was no free slot; restore and return false

		// however: if the count (temporarily) fell below zero then
		// other threads restoring the count won't 'notify'. Only
		// the last one leaving this section should!

		if ( WATERMARK == slots_.fetch_add(1, memory_order_release) ) {
			// if we are going to wait then 'processEvent' will poll immediately
			// and since we are not blocking yet 'notify' would do nothing...
			if ( ! wait )
				notify();
		}
	} while ( wait && evSet_->processEvent( wait, abs_timeout ) );

	return false;
}

void CEventBufSync::putSlot()
{
	if ( WATERMARK == slots_.fetch_add(1, memory_order_release) ) {
		// wake up folks who are blocking in 'processEvent'
		notify();
	}
}


class CBufQueue : public IBufQueue, protected CBufQueueBase {
private:
	unsigned      n_;
	bool          isUp_;
	CEventBufSync rd_sync_impl_;
	CEventBufSync wr_sync_impl_;

	IBufSync     *rd_sync_;
	IBufSync     *wr_sync_;

	CBufQueue & operator=(const CBufQueue &orig); // must not assign
	CBufQueue(const CBufQueue &orig);             // must not copy

protected:
	BufChain pop(bool wait, const CTimeout * abs_timeout);
	bool     push(BufChain b, bool wait, const CTimeout *abs_timeout);

public:
	CBufQueue(size_type n);

	virtual BufChain pop(const CTimeout *abs_timeout)
	{
		return pop(true, abs_timeout);
	}

	virtual BufChain tryPop()
	{
		return pop(false, 0);
	}

	virtual bool isFull()
	{
		return wr_sync_->getAvailSlots() <= 0;
	}

	virtual bool isEmpty()
	{
		return rd_sync_->getAvailSlots() <= 0;
	}


	virtual bool     push(BufChain b, const CTimeout *abs_timeout)
	{
		return push(b, true, abs_timeout);
	}

	virtual bool     tryPush(BufChain b)
	{
		return push(b, false, 0);
	}

	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout)
	{
	CTimeout rval;
		rd_sync_->getAbsTimeout( &rval, rel_timeout );
		return rval;
	}

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)
	{
	CTimeout rval;
		wr_sync_->getAbsTimeout( &rval, rel_timeout );
		return rval;
	}

	virtual IEventSource *getReadEventSource()
	{
		return rd_sync_->getEventSource();
	}

	virtual IEventSource *getWriteEventSource()
	{
		return wr_sync_->getEventSource();
	}

	virtual void shutdown();

	virtual void startup();

	virtual ~CBufQueue();
};



CBufQueue::CBufQueue(size_type n)
: CBufQueueBase(n),
  n_(n),
  isUp_(true),
  rd_sync_impl_(0),
  wr_sync_impl_(n),
  rd_sync_( &rd_sync_impl_ ),
  wr_sync_( &wr_sync_impl_ )
{
}

BufQueue IBufQueue::create(unsigned n)
{
	return make_shared<CBufQueue>(n);
}

CBufQueue::~CBufQueue()
{
	// since there are raw pointers stored in the queue
	// we must extract the shared_ptr ownership from all
	// stored elements here.
	shutdown();
}

void CBufQueue::shutdown()
{
unsigned   wi;

	if ( ! isUp_ )
		return;

	isUp_ = false;

	wi = 0;
	while ( wi < n_ ) {
		// eat up writer slots
		while ( wr_sync_->getSlot( false, NULL ) )
			wi++;
		// don't use tryPop() here because 
		while ( tryPop() )
			;
	}
}

void CBufQueue::startup()
{
unsigned i;

	if ( isUp_ )
		return;

	isUp_ = true;

	for ( i=0; i<n_; i++ )
		wr_sync_->putSlot();

}

bool CBufQueue::push(BufChain b, bool wait, const CTimeout *abs_timeout)
{

	// wait for a slot
	if ( ! wr_sync_->getSlot( wait, abs_timeout ) ) {
		return false;
	}

	IBufChain::take_ownership(b);
	// 1 ref inside the BufChain obj
	// 1 ref in our local var
	// (*owner) has been reset

	if ( bounded_push( b.get() ) ) {
		rd_sync_->putSlot();
		return true;
	} else {

		wr_sync_->putSlot();

		// enqueue failed -- clear ownership stored
		// in buffer
		b->yield_ownership();

		throw InternalError("Queue inconsistency???");
	}
	return false;
}

BufChain CBufQueue::pop(bool wait, const CTimeout *abs_timeout)
{

	if ( rd_sync_->getSlot(wait, abs_timeout) ) {
		IBufChain *raw_ptr;
		if ( !CBufQueueBase::pop( raw_ptr ) ) {
			throw InternalError("FATAL ERROR -- unable to pop even though we decremented the semaphore?");
		}
		BufChain rval = raw_ptr->yield_ownership();
		wr_sync_->putSlot();
		return rval;
	}

	// timeout (or trywait failed)
	return BufChain( reinterpret_cast<BufChain::element_type *>(0) );
}

void IBufSync::clockRealtimeGetAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout)
{

	if ( ! rel_timeout || rel_timeout->isIndefinite() ) {
		*abs_timeout = TIMEOUT_INDEFINITE;
		return;
	}

	if ( clock_gettime( CLOCK_REALTIME, &abs_timeout->tv_ ) )
		throw InternalError("clock_gettime failed", errno);

	if ( ! rel_timeout->isNone() ) {
		*abs_timeout += *rel_timeout;
	}
}
