#include <cpsw_shared_obj.h>
#include <cpsw_event.h>
#include <cpsw_mutex.h>
#include <vector>

#include <pthread.h>
#include <errno.h>

#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>

using std::vector;
using boost::make_shared;
using boost::weak_ptr;

class CondInitFailed   {};
class CondWaitFailed   {};
class CondSignalFailed {};

class CCond {
private:
	pthread_cond_t cond_;

	CCond(const CCond&);
	CCond & operator=(const CCond&);
public:
	CCond()
	{
		if ( pthread_cond_init( &cond_, NULL ) )
			throw CondInitFailed();
	}

	~CCond()
	{
		pthread_cond_destroy( &cond_ );
	}

	pthread_cond_t *getp() { return &cond_; }
};

class CEventSet : public IEventSet, public CShObj {
protected:
	typedef std::pair<IEventSource*, IEventHandler *> Binding;
	typedef shared_ptr<CEventSet>                     EventSetImpl;
private:
	vector<Binding>       srcs_;

	CMtx                  mtx_;
	CCond                 cnd_;

	CEventSet(const CEventSet &orig);
	CEventSet operator=(const CEventSet &orig);

	bool tryRemoveSrc(unsigned index);

public:

	CEventSet(Key &k):CShObj(k), mtx_("EVS") {}

	virtual void add(IEventSource *src, IEventHandler *h)
	{
	unsigned i;

	// always lock source before the event set
	CMtx::lg guard( &src->mtx_ );

		// remove current association -- may call 'del'
		// and recursively lock the source
		src->clrEventSet();

		{CMtx::lg guard( &mtx_ );

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

		{CMtx::lg guard( &mtx_ );

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
	unsigned i;
	unsigned attempt = 0;

		// can't lock the source yet -- unknown
	retry:

		{CMtx::lg guard( &mtx_ );

			for ( i=0; i<srcs_.size(); i++ ) {
				if ( srcs_[i].second == h ) {

					// since we must acquire the source lock after the
					// event set lock we should only try and back off
					// if we can't acquire the source

					if ( ! tryRemoveSrc( i ) ) {
						if ( ++attempt > 10 ) {
							throw InternalError("Unable to remove event handler -- starved");
						}
						goto retry;
					}

				}
			}
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

	CMtx::lg guard( &mtx_ );

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
					st = pthread_cond_timedwait( cnd_.getp(), mtx_.getp(), &abs_timeout->tv_ );
				} else {
					st = pthread_cond_wait( cnd_.getp(), mtx_.getp() );
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
		CMtx::lg guard( &mtx_ );

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

void IEventSource::setEventSet(EventSet eventSet)
{
	if ( eventSet_ ) {
		throw InternalError("Source already bound to an event set");
	}
	eventSet_ = eventSet;
}

void IEventSource::clrEventSet(EventSet eventSet)
{
	if ( eventSet_ != eventSet )
		throw InternalError("removing unexpected event set");
	eventSet_.reset();
}

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

bool CEventSet::tryRemoveSrc(unsigned index)
{
IEventSource *src = srcs_[index].first;
unsigned      j;

	try { CMtx::lg guard( &src->mtx_, false /* dont' block */);

		for ( j=index+1; j<srcs_.size(); j++ ) {
			srcs_[j-1] = srcs_[j];
		}
		srcs_.pop_back();
		src->clrEventSet( getSelfAs<EventSetImpl>() );
		return true;
	} catch ( CMtx::MutexBusy ) {
		struct timespec t;
		t.tv_sec  = 0;
		t.tv_nsec = 10000000;
		nanosleep( &t, NULL );
	}
	return false;
}

CEventSet::~CEventSet()
{
unsigned i;
unsigned attempt = 0;

	// cannot lock the source first since it is unknown
retry:

	{ CMtx::lg guard( &mtx_ );

		while ( ( i = srcs_.size()) > 0 ) {

			// try to acquire the source lock and back-off if that fails

			if ( ! tryRemoveSrc( i-1 ) ) {
				if ( ++attempt > 10 ) {
					throw InternalError("Unable to remove event source -- starved");
				}
				goto retry;
			}

			attempt = 0;
		}
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
