 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_shared_obj.h>
#include <cpsw_event.h>
#include <cpsw_mutex.h>
#include <cpsw_condvar.h>
#include <vector>

#include <pthread.h>
#include <errno.h>

#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>

using std::vector;
using boost::make_shared;
using boost::weak_ptr;

class CEventSet : public IEventSet, public CShObj {
protected:
	typedef std::pair<IEventSource*, IEventHandler *> Binding;
	typedef shared_ptr<CEventSet>                     EventSetImpl;
private:
	vector<Binding>       srcs_;

	CMtx                  mutx_;
	CCond                 cnd_;

	CEventSet(const CEventSet &orig);
	CEventSet operator=(const CEventSet &orig);

	bool tryRemoveSrc(IEventHandler *);

public:

	CEventSet(Key &k):CShObj(k), mutx_("EVS") {}

	virtual void add(IEventSource *src, IEventHandler *h)
	{
	unsigned i;

	// always lock source before the event set
	CMtx::lg guard( &src->mtx_ );

		// remove current association -- may call 'del'
		// and recursively lock the source
		src->clrEventSet();

		{CMtx::lg guard( &mutx_ );

			for ( i=0; i<srcs_.size(); i++ ) {
				if ( srcs_[i].first == src ) {
					// replace existing
					srcs_[i] = Binding(src,h);
					src->setEventSet( getSelfAs<EventSetImpl>() );
					return;
				}
			}
			srcs_.push_back( Binding(src,h) );
			src->setEventSet( getSelfAs<EventSetImpl>() );
		}
	}

	virtual void del(IEventSource *src)
	{
	unsigned i,j;

	// always lock source before the event set
	CMtx::lg guard( &src->mtx_ );

		{CMtx::lg guard( &mutx_ );

			for ( i=0; i<srcs_.size(); i++ ) {
				if ( srcs_[i].first == src ) {
					for ( j=i+1; j<srcs_.size(); j++ ) {
						srcs_[j-1] = srcs_[j];
					}
					srcs_.pop_back();
					src->clrEventSet( getSelfAs<EventSetImpl>() );
					return;
				}
			}
		}
	}

	virtual void del(IEventHandler *h)
	{
	unsigned attempt = 0;

		while ( ! tryRemoveSrc( h ) ) {
			if ( ++attempt > 10 ) {
				throw InternalError("Unable to remove event handler -- starved");
			}
			struct timespec t;
			t.tv_sec  = 0;
			t.tv_nsec = 10000000;
			nanosleep( &t, NULL );
		}
	}


	virtual Binding *pollAll()
	{
	unsigned i;
	Binding *rval;

		for ( i=0; i<srcs_.size(); i++ ) {
			rval = &srcs_[i];
			if ( rval->second->isEnabled() && rval->first->poll() ) {
				return rval;
			}
		}
		return NULL;
	}

	virtual Binding *getEvent(bool wait, const CTimeout *abs_timeout)
	{
	int         st;
	Binding  *rval;

	CMtx::lg guard( &mutx_ );

		if ( wait && abs_timeout ) {
			if ( abs_timeout->isNone() ) {
				wait = false;
			} else if ( abs_timeout->isIndefinite() ) {
				abs_timeout = NULL;
			}
		}

		while ( ! (rval = pollAll()) ) {

			if ( wait ) {
				if ( abs_timeout ) {
					st = pthread_cond_timedwait( cnd_.getp(), mutx_.getp(), &abs_timeout->tv_ );
				} else {
					st = pthread_cond_wait( cnd_.getp(), mutx_.getp() );
				}

				if ( st ) {
					if ( ETIMEDOUT == st )
						return NULL;

					throw CondWaitFailed();
				}

			} else {
				return NULL;
			}

		}

		return rval;
	}

	virtual bool processEvent(bool wait, const CTimeout *abs_timeout)
	{
		Binding *b;

		if ( (b = getEvent(wait, abs_timeout)) ) {
			b->first->handle( b->second );
			return true;
		}
		return false;
	}

	virtual void notify()
	{
		// an event condition could have become true
		// after the receiving thread last polled
		// but before it went to sleep. In order to
		// synchronize with this condition we
		// must take the mutex!
		CMtx::lg guard( &mutx_ );

		if ( pthread_cond_signal( cnd_.getp() ) )
			throw CondSignalFailed();
	}

	virtual void getAbsTimeout(CTimeout *abs_timeout, const CTimeout *rel_timeout)
	{
		if ( clock_gettime( CLOCK_REALTIME, & abs_timeout->tv_ ) )
			throw InternalError("clock_gettime failed");
		if ( rel_timeout )
			*abs_timeout += *rel_timeout;
	}

	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout)
	{
	CTimeout rval;
		if ( clock_gettime( CLOCK_REALTIME, & rval.tv_ ) )
			throw InternalError("clock_gettime failed");
		if ( rel_timeout )
			rval += *rel_timeout;
		return rval;
	}

	virtual void getAbsTime(CTimeout *abs_time)
	{
		if ( clock_gettime( CLOCK_REALTIME, & abs_time->tv_ ) )
			throw InternalError("clock_gettime failed");
	}

	virtual ~CEventSet();

	static EventSetImpl create();
};

EventSet IEventSet::create()
{
	return CEventSet::create();
}

CEventSet::EventSetImpl CEventSet::create()
{
	return CShObj::create<EventSetImpl>();
}

IEventSource::~IEventSource()
{
CMtx::lg guard( &mtx_ );
	if ( eventSet_ )
		eventSet_->del( this );
}

// NOTE: caller must hold the Source's mtx_
void IEventSource::setEventSet(EventSet eventSet)
{
	if ( eventSet_ ) {
		throw InternalError("Source already bound to an event set");
	}
	eventSet_ = eventSet;
}

// NOTE: caller must hold the Source's mtx_
void IEventSource::clrEventSet(EventSet eventSet)
{
	if ( eventSet_ != eventSet )
		throw InternalError("removing unexpected event set");
	eventSet_.reset();
}

// NOTE: caller must hold the Source's mtx_
void IEventSource::clrEventSet()
{
	if ( eventSet_ )
		eventSet_->del( this );
	eventSet_.reset();
}

void IEventSource::notify()
{
CMtx::lg guard( &mtx_ );

	if ( eventSet_ )
		eventSet_->notify();
}

bool CIntEventSource::checkForEvent()
{
int sent = sentVal_.exchange( 0, memory_order_acquire );
	if ( sent ) {
		setEventVal( sent );
		return true;
	}
	return false;
}

// we use tryRemoveSrc() if we are in a situation where
// we cannot acquire the source lock prior to the
// event-set lock.

bool CEventSet::tryRemoveSrc(IEventHandler *h)
{
IEventSource *src;
unsigned     i,j;

// can't lock the source yet -- unknown

CMtx::lg guard( &mutx_ );

	for ( i=0; i<srcs_.size(); i++ ) {

		if ( srcs_[i].second == h ) {
			src = srcs_[i].first;

			try {

				CMtx::lg guard( &src->mtx_, false /* dont' block */);

				for ( j=i+1; j<srcs_.size(); j++ ) {
					srcs_[j-1] = srcs_[j];
				}
				srcs_.pop_back();
				src->clrEventSet( getSelfAs<EventSetImpl>() );

				return true;

			} catch ( CMtx::MutexBusy ) {
				return false;
			}
		}

	}

	return true;
}

CEventSet::~CEventSet()
{
	// DONT remove event sources; the set must be empty since
	// every active source holds a reference to this event set
	// which is released when the source is destroyed. The
	// source destructor already removes itself from the event set.
	if ( srcs_.size() > 0 ) {
		throw InternalError("Event set not empty during destruction!");
	}
}

CIntEventSource::CIntEventSource()
: sentVal_(0)
{
}

void CIntEventSource::sendEvent(int val)
{
	if ( val ) {
		sentVal_.store( val, memory_order_release );
		notify();
	}
}

void CIntEventHandler::handle(IIntEventSource *src)
{
int gotVal = src->getEventVal();
	if ( ! gotVal ) {
		throw InternalError("Unexpected Int event value received");
	}
	receivedVal_.store( gotVal, memory_order_release );
}

int CIntEventHandler::receiveEvent(EventSet evSet, bool wait, const CTimeout *abs_timeout)
{
	if ( evSet->processEvent(wait, abs_timeout) ) {
		return receivedVal_.load( memory_order_acquire );
	}
	return 0;
}

int CIntEventHandler::receiveEvent(IIntEventSource *src, bool wait, const CTimeout *abs_timeout)
{
EventSet evSet = IEventSet::create();
	evSet->add( src, this );
	return receiveEvent(evSet, wait, abs_timeout);
}
