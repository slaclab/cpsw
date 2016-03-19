#include <cpsw_event.h>

#include <vector>

#include <pthread.h>
#include <errno.h>

#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>

using std::vector;
using boost::make_shared;
using boost::weak_ptr;

class CondInitFailed {};
class CondWaitFailed {};
class CondSignalFailed {};

class CMtx {
	pthread_mutex_t m_;
	const char *nam_;

private:
	CMtx(const CMtx &);
	CMtx & operator=(const CMtx&);
public:

	class lg {
		private:
			CMtx *mr_;

			lg operator=(const lg&);

			lg(const lg&);

		public:
			lg(CMtx *mr)
			: mr_(mr)
			{
				mr_->l();
			}

			~lg()
			{
				mr_->u();
			}

		friend class CMtx;
	};


	CMtx(const char *nam):nam_(nam)
	{
		if ( pthread_mutex_init( &m_, NULL ) ) {
			throw InternalError("Unable to create mutex", errno);
		}
	}

	~CMtx()
	{
		pthread_mutex_destroy( &m_ );
	}

	void l() 
	{
		if ( pthread_mutex_lock( &m_ ) ) {
			throw InternalError("Unable to lock mutex");
		}
	}

	void u()
	{
		if ( pthread_mutex_unlock( &m_ ) ) {
			throw InternalError("Unable to unlock mutex");
		}
	}

	pthread_mutex_t *getp() { return &m_; }
};


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

class CEventSet : public IEventSet {
protected:
	typedef std::pair<IEventSource*, IEventHandler *> Binding;
private:
	weak_ptr<CEventSet>   self_;
	vector<Binding>       srcs_;

	CMtx                  mtx_;
	CCond                 cnd_;

public:

	CEventSet():mtx_("EVS") {}

	virtual void add(IEventSource *src, IEventHandler *h)
	{
	unsigned i;
		for ( i=0; i<srcs_.size(); i++ ) {
			if ( srcs_[i].first == src ) {
				// replace existing
				srcs_[i] = Binding(src,h);
				src->setEventSet( EventSet(self_) );
				return;
			}
		}
		srcs_.push_back( Binding(src,h) );
		src->setEventSet( EventSet(self_) );
	}

	virtual void del(IEventSource *src)
	{
	unsigned i,j;
		for ( i=0; i<srcs_.size(); i++ ) {
			if ( srcs_[i].first == src ) {
				for ( j=i+1; j<srcs_.size(); j++ ) {
					srcs_[j-1] = srcs_[j];
				}
				srcs_.pop_back();
				src->setEventSet( EventSet() );
				return;
			}
		}
	}

	virtual void del(IEventHandler *h)
	{
	unsigned i,j;
		for ( i=0; i<srcs_.size(); i++ ) {
			if ( srcs_[i].second == h ) {
				IEventSource *src = srcs_[i].first;
				for ( j=i+1; j<srcs_.size(); j++ ) {
					srcs_[j-1] = srcs_[j];
				}
				srcs_.pop_back();
				src->setEventSet( EventSet() );
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

	static shared_ptr<CEventSet> create();
};

EventSet IEventSet::create()
{
	return CEventSet::create();
}

shared_ptr<CEventSet> CEventSet::create()
{
shared_ptr<CEventSet> rval = make_shared<CEventSet>();
	rval->self_ = rval;
	return rval;
}

IEventSource::~IEventSource()
{
	if ( eventSet_ )
		eventSet_->del( this );
}

void IEventSource::setEventSet(EventSet eventSet)
{
	if ( eventSet_ && eventSet_ != eventSet ) {
		// if this source is already bound (and just the
		// handler is changed then we don't want to remove
		eventSet_->del( this );
	}
	eventSet_ = eventSet;
}

void IEventSource::notify()
{
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


#if 0
class RXEvent : public IEvent, public IEventSource {
	Queue q_;
	Buf b_;

	virtual IEvent *poll()
	{
		if ( b_ = q_->tryPop() )
			return this;
		return NULL;
	}

	virtual void handle()
	{
		
	}
};

sync {
	int avail_;
	EventSet rx_event_set;
	putSlot()
	{ 	
	if ( avail_.fetch_add(1) == 0 )
		rx_event_set->notify();
	}
	getSlot()
	{
		if ( avail_.fetch_sub(1) == 0 )
			avail_.fetch_add(1);
		return false;
		return true;
	}
}

queue::push()
{
	rx_inqueue->wr_sync_->getSlot();
    enqueue;
	rx_inqueue->rd_sync_->putSlot();
}

queue::pop()
{
	if ( rx_inqueue->rd_sync_->getSlot() ) {
		rval = dequeue()
		rx_inqueue->wr_sync_->putSlot();
	}
  	return rval; 
}

class XX : public IEventSet {

	virtual void handleRx(Buf b);

	class RXEvent : public IEvent {
		private:
			XX *xx;
		public:
			RXEvent(XX *xx):xx(xx){}

			virtual void handle()
			{
				xx->handleRx( 
			}
	};
};
#endif
