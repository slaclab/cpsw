#ifndef CPSW_EVENT_SETS_H
#define CPSW_EVENT_SETS_H

#include <cpsw_api_timeout.h>
#include <cpsw_error.h>
#include <cpsw_mutex.h>

#include <boost/shared_ptr.hpp>
#include <boost/atomic.hpp>

using boost::shared_ptr;
using boost::atomic;
using boost::memory_order_acquire;
using boost::memory_order_release;

// The 'event' facility allows for a thread waiting until
// any of a set of conditions become true (similar to unix poll/select).
//
//  - An 'EventSource' defines a condition that constitutes an 'event'.
//    It must implement a 'checkForEvent()' method which polls for the
//    condition and must return true/false.
//
//  - An 'EventHandler' which implements an action to be executed when
//    the condition defined by the associated 'EventSource' becomes true.
//
//  - An 'EventSet' which is a (dynamic) set of associations of
//    'EventSource's with 'EventHandlers'.
//
// Typically, the facility is used to synchronize multiple threads.
//
//  1. one thread associated per 'event source'
//  2. a single thread which waits for events and executes handlers.
//
// The processing thread '2' loops waiting for events and executes
// handlers
//
// while (1) {
//           if ( ! eventSet->processEvent( wait, timeout ) ) {
//               /* deal with timeouts */
//           }
// }
//
// An event source typically derives from CIntEventSource:
//
// class MyClass : ICIntEventSource {
//
//   virtual void doProcess {
//        // want to wake up target here
//        sendEvent(44);
//   }
//


// Other classes of event sources to be added here...
// Look for a '####' pattern to find other locations
// that need to be modified.
class IIntEventSource;

class IEventSet;
typedef shared_ptr<IEventSet> EventSet;

class IEventHandler {
private:
	bool isEnabled_;
public:
	IEventHandler() : isEnabled_(true) {}

	virtual bool isEnabled()
	{
		return isEnabled_;
	}

	virtual bool disable()
	{
	bool wasEn = isEnabled();
		isEnabled_ = false;
		return wasEn;
	}

	virtual void enable()
	{
		isEnabled_ = true;
	}

	// '####' Every new class of event source needs a 'handle'
	// method (visitor pattern)
	//
	// virtual void handle(XXXEventSource *es) { throw InternalError(""); }
	//
	// and every handler that can deal with such a source needs
	// to implement a 'handle(XXXEventSource*)' method.

	virtual void handle(IIntEventSource *es)
	{
		throw InternalError("No event handler implemented for this source");
	}
};

class IEventSource {
private:
	bool      pending_;
	EventSet  eventSet_;
	CMtx      mtx_;

	void setEventSet(EventSet newSet);
	void clrEventSet(EventSet curSet);
	void clrEventSet();

	friend class CEventSet;

protected:
	// Every subclass must add
	//   virtual void dispatch(IEventHandler *h) { h->handle( this ); }
	// for the visitor pattern to work
	virtual void dispatch(IEventHandler *h) = 0;

	// Subclass must implement this to check for event condition.
	// It may also store associated data to be retrieved
	// by the handler.
	virtual bool checkForEvent()            = 0;

public:
	IEventSource()
	: pending_(false),
	  mtx_( CMtx::AttrRecursive(), "IEventSource" )
	{
	}

	// 'poll()' and 'handle()' MUST be called from the same
	// thread and/or be properly synchronized.
	// After 'poll' returns 'true' for this IEventSource the
	// source is in a 'pending' state. 'handle()' must then
	// be executed exactly once.

	// Note that 'poll' is executed with a mutex locked
	virtual bool poll()
	{
		if ( ! pending_ ) {
			pending_ = checkForEvent();		
		}
		return pending_;
	}

	// Just read the 'pending' flag w/o checking for a new
	// event.
	virtual bool isPending()
	{
		return pending_;
	}

	virtual void handle(IEventHandler *h)
	{
		if ( pending_ ) {
			dispatch( h );
			pending_ = false;
		}
	}

	// notify the eventSet bound to this source
	virtual void notify();

	virtual ~IEventSource();
};

class IIntEventSource : public IEventSource {
private:
	int val_;
protected:
	virtual void dispatch(IEventHandler *h) { h->handle( this ); }

	virtual void setEventVal(int v)
	{
		if ( isPending() ) {
			throw InternalError("Event already pending");
		}
		val_ = v;
	}

public:
	virtual int  getEventVal()
	{
		if ( ! isPending() )
			throw InternalError("No event pending");
		return val_;
	}
};

class CIntEventSource : public IIntEventSource {
private:

	atomic<int> sentVal_;

protected:

	virtual bool checkForEvent();

public:

	CIntEventSource();

	// Note: if this source is not a member of the
	// target event set then nothing happens (at least
	// one blocked thread in the target set is woken
	// up and poll but the won't 'see' this source).
	virtual void sendEvent(int val);
};

// Don't use shared_ptr for referencing EventSources or Handlers.

// The Entity using the EventSet (processEvent) usually also implements
// the handler and needs some kind of reference to the source.
// Avoid creating circular references by using straight pointers here.
//
// Double lines represent shared_ptr, single-lines ordinary pointers:
//
//                =========== OBJECT implementing HANDLER + event loop
//                V          ||
//             SOURCE        ||HANDLER
//             ||    ^       || ^
//   (for      ||    | for   || |
//    'notify')||    | 'poll'\/ | (dispatch)
//              ====>  EventSet
//
// Usually: the SOURCE uses a shared_ptr to the EventSet (so it can 'notify')
//          and the EventSet uses a straight pointer back (so it can 'poll')
//          
//          The Entity implementing the HANDLER normally also executes
//          the event loop (processEvent) and holds a shared_ptr to
//          the EventSet. The EventSet uses a straight pointer back to the
//          handler (for dispatching the handler).
//
//          Furthermore: the Entity implementing the HANDLER often also
//          uses the object containing the SOURCE via a shared_ptr.
//          If this is not the case, THEN some care but be taken, e.g.,
//          by letting the destructor of OBJECT remove it's handler
//          from the EventSet (which is a good idea anyways).
class IEventSet {
public:
	// wait (up to 'abs_timeout' or forever if NULL is passed)
	// for one event and executes the handler associated with
	// the source which posted the event.
	// Returns 'true' if an event was handled, 'false' if a timeout
	// occured.
	virtual bool processEvent(bool wait, const CTimeout *abs_timeout) = 0;

	virtual void add(IEventSource *, IEventHandler *h)                = 0;
	virtual void del(IEventSource  *)                                 = 0;
	virtual void del(IEventHandler *)                                 = 0;

	// to be called by 
	virtual void notify()                                             = 0;

	virtual ~IEventSet() {}

	virtual void     getAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout) = 0;
	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout)                    = 0;
	virtual void     getAbsTime(CTimeout *abs_time)                                    = 0;

	static EventSet create();
};

#endif
