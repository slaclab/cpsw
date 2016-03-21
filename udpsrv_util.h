#ifndef Q_HELPER_H
#define Q_HELPER_H

#include <boost/smart_ptr.hpp>
#include <boost/atomic.hpp>

// simplified implementation of buffers etc.
// for use by udpsrv to have a testbed which
// is independent from the main code base...

#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <cpsw_error.h>

#include <cpsw_api_timeout.h>

#include <cpsw_event.h>

#include <stdio.h>


using boost::shared_ptr;
using boost::atomic;

class IEventBufSync;
typedef shared_ptr<IEventBufSync> EventBufSync;

class IBufSync;
typedef shared_ptr<IBufSync> BufSync;

class IBuf;
typedef shared_ptr<IBuf> Buf;

class IBufChain;
typedef shared_ptr<IBufChain> BufChain;

class IBufQueue;
typedef shared_ptr<IBufQueue> BufQueue;


class IBufSync {
public:
	virtual bool getSlot(bool, const CTimeout *)  = 0;
	virtual void putSlot()                        = 0;
	virtual unsigned getMaxSlots()                = 0;
	virtual unsigned getAvailSlots()              = 0;
	virtual ~IBufSync() {}
};

class IEventBufSync : public IBufSync {
public:
	virtual IEventSource *getEventSource() = 0;
	virtual ~IEventBufSync() {}
};

class CSemBufSync : public IBufSync {
private:
	unsigned ini_;
	sem_t    sem_;
public:
	CSemBufSync(unsigned val = 0)
	: ini_(val)
	{
		if ( sem_init( &sem_, 0, ini_ ) ) {
			throw InternalError("Unable to create semaphore", errno);
		}
	}

	CSemBufSync(const CSemBufSync &orig)
	: ini_(orig.ini_)
	{
		if ( sem_init( &sem_, 0, ini_ ) ) {
			throw InternalError("Unable to create semaphore", errno);
		}
	}

	virtual bool getSlot(bool wait, const CTimeout *abs_timeout)
	{
	int sem_stat;
		if ( !wait ) {
			sem_stat = sem_trywait( &sem_ );
		} else if ( ! abs_timeout ) {
			sem_stat = sem_wait( &sem_ );
		} else {
			sem_stat = sem_timedwait( &sem_, &abs_timeout->tv_);
		}

		if ( sem_stat ) {
			if ( EAGAIN == errno  || ETIMEDOUT == errno )
				return false;
			throw InternalError("Unable to get semaphore", errno);
		}
		return true;
	}

	virtual unsigned getMaxSlots()   { return ini_; }

	virtual unsigned getAvailSlots()
	{
	int v;
		if ( sem_getvalue( &sem_, &v ) ) {
			throw InternalError("Unable to get semaphore value");
		}
		return v < 0 ? 0 : (unsigned)v;
	}

	virtual void putSlot()
	{
		if ( sem_post( &sem_ ) ) {
			throw InternalError("Unable to post semaphore", errno);
		}
	}

	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout)
	{
		struct timespec ts;

		if ( ! rel_timeout ) {
			ts.tv_sec = (1<<30) + ((1<<30)-1);
			ts.tv_nsec = 0;
			return ts;
		}

		if ( clock_gettime( CLOCK_REALTIME, &ts ) )
			throw InternalError("clock_gettime failed", errno);

		ts.tv_nsec += rel_timeout->tv_.tv_nsec;
		if ( ts.tv_nsec >= 1000000000 ) {
			ts.tv_nsec -= 1000000000;
			ts.tv_sec  += 1;
		}
		ts.tv_sec += rel_timeout->tv_.tv_sec;

		return ts;
	}

	virtual BufSync clone()
	{
	CSemBufSync *p = new CSemBufSync( *this );
		return BufSync( p );
	}

	virtual ~CSemBufSync()
	{
		sem_destroy( &sem_ );
	}

	static BufSync create(unsigned depth=0);
};

class CEventBufSync : public IIntEventSource, public IEventBufSync, public IEventHandler {
private:
	unsigned    depth_;
	int         water_;
	atomic<int> slots_;
	EventSet    evSet_;
protected:
	CEventBufSync(unsigned depth, unsigned water)
	: depth_(depth),
	  water_(water > depth - 1 ? depth - 1 : water),
	  slots_(depth),
	  evSet_(IEventSet::create())
	{
		evSet_->add( (IIntEventSource*)this, (IEventHandler*)this );
	}
public:
	virtual bool getSlot(bool, const CTimeout *);
	virtual void putSlot();
	virtual ~CEventBufSync() {}

	virtual IEventSource *getEventSource() { return this; }

	virtual void handle(IIntEventSource *s) {}

	virtual unsigned getMaxSlots()   { return depth_;        }
	// this might temporarily yield too small a value (if 
	// called while other threads are in getSlot().
	virtual unsigned getAvailSlots()
	{
		int v = slots_.load(memory_order_acquire);
		return v < 0 ? 0 : (unsigned)v;
	}

	virtual unsigned getWatermark() { return water_; }

	virtual bool checkForEvent() {
		int avail = getAvailSlots();
		if ( avail > water_ ) {
			setEventVal( avail );
			return true;
		}
		return false;
	}

	static EventBufSync create(unsigned depth=0, unsigned water = 0);
};

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


class IBuf {
public:
	virtual uint8_t* getPayload()  = 0;
	virtual size_t   getSize()     = 0;
	virtual unsigned getCapacity() = 0;

	virtual void     setPayload(uint8_t*) = 0;
	virtual void     setSize(unsigned) = 0;

	virtual ~IBuf() {}

	static Buf get();
	static void dumpStats();

	static const int CAPA_ETH_HDR = 128;
	static const int CAPA_ETH_BIG = 1500;
};

class IBufChain {
public:
	virtual Buf getHead()      = 0;
	virtual unsigned getLen()  = 0;
	virtual size_t   getSize() = 0;
	virtual Buf createAtHead(size_t) = 0;

	virtual unsigned extract(void *buf, unsigned off, unsigned size) = 0;
	virtual unsigned insert(void *buf, unsigned off, unsigned size) = 0;

	virtual ~IBufChain() {}

	static BufChain create();

};

class IBufQueue {
public:
	virtual BufChain  pop(const CTimeout *abs_timeout)             = 0;
	virtual BufChain  tryPop()                                     = 0;
	virtual bool push(BufChain, const CTimeout *abs_timeout)       = 0;
	virtual bool tryPush(BufChain)                                 = 0;

	// shutdown and startup must be executed from a single thread only
	virtual void shutdown()                                        = 0;
	virtual void startup()                                         = 0;

	virtual bool isFull()                                          = 0;

	virtual ~IBufQueue() {}

	virtual IEventSource *getReadEventSource()                     = 0;
	virtual IEventSource *getWriteEventSource()                    = 0;

	static BufQueue create(unsigned depth, unsigned wwater = 0);
};

class IProtoPort;
typedef shared_ptr<IProtoPort> ProtoPort;

class IProtoPort {
public:
	virtual BufChain pop(const CTimeout *)                 = 0;
	virtual BufChain tryPop()                              = 0;

	virtual IEventSource *getReadEventSource()             = 0;

	virtual void attach(ProtoPort upstream)                     = 0;

	virtual bool push(BufChain, const CTimeout *)          = 0;
	virtual bool tryPush(BufChain)                         = 0;

	virtual ~IProtoPort() {}

	virtual void start()                                   = 0;

	static ProtoPort create(bool isServer_);
};

class IUdpPort;
typedef shared_ptr<IUdpPort> UdpPort;

class IUdpPort : public IProtoPort {
public:
	virtual BufChain pop(const CTimeout *)       = 0;
	virtual BufChain tryPop()                    = 0;

	virtual IEventSource *getReadEventSource()
	{
		return NULL;
	}

	virtual void attach(ProtoPort upstream)
	{
		throw InternalError("This must be a 'top' port");
	}

	virtual bool push(BufChain, const CTimeout *) = 0;
	virtual bool tryPush(BufChain)                = 0;

	virtual unsigned isConnected()                = 0;


	virtual void start()                          = 0;

	virtual ~IUdpPort() {}

	static UdpPort create(const char *ina, unsigned port);
};

class ILoopbackPorts;
typedef shared_ptr<ILoopbackPorts> LoopbackPorts;

class ILoopbackPorts {
public:
	virtual ProtoPort getPortA() = 0;
	virtual ProtoPort getPortB() = 0;

	static LoopbackPorts create(unsigned depth, unsigned lossPercent, unsigned garbl);
};

class ISink: public IProtoPort {
public:
	static ProtoPort create(const char *name);
};

#endif
