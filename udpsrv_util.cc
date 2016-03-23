#include <udpsrv_util.h>
#include <cpsw_thread.h>

#include <boost/make_shared.hpp>
#include <boost/atomic.hpp>
#include <boost/weak_ptr.hpp>

#include <stdio.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <vector>

using std::vector;
using boost::atomic;
using boost::weak_ptr;
using boost::make_shared;
using boost::memory_order_acquire;
using boost::memory_order_release;
using boost::static_pointer_cast;

class IEventBufSync;
typedef shared_ptr<IEventBufSync> EventBufSync;

class IBufSync {
public:
	virtual bool getSlot(bool, const CTimeout *)                = 0;
	virtual void putSlot()                                      = 0;
	virtual unsigned getMaxSlots()                              = 0;
	virtual unsigned getAvailSlots()                            = 0;
	virtual CTimeout getAbsTimeout(const CTimeout *rel_timeout) = 0;
	virtual ~IBufSync() {}
};

typedef shared_ptr<IBufSync> BufSync;

class IEventBufSync : public IBufSync {
public:
	virtual IEventSource *getEventSource() = 0;
	virtual ~IEventBufSync() {}
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


	static EventBufSync create(unsigned depth=0, unsigned water = 0);
};



class CBuf : public IBuf, public IBufChain {
private:
	uint8_t  dat_[1500];
	unsigned off_;
	unsigned len_;
	unsigned siz_;
	unsigned chl_;
	weak_ptr<CBuf> self_;

	CBuf()
	: off_(32),
	  len_(0),
	  chl_(0)
	{
	}

	CBuf operator=(const CBuf &);

	class FL {
	private:
		void  *h_;

		unsigned totl_, free_;

		CMtx mtx_;

		FL():h_(NULL), totl_(0), free_(0), mtx_("FL") {}

	public:
		CBuf *get()
		{
		CMtx::lg guard( &mtx_ );

		CBuf *rval;
		void *mem;
			if ( (mem = h_) ) {
				void **nextp = reinterpret_cast<void**>( mem );
				h_ = *nextp;
				rval = new (mem) CBuf();
				free_--;
			} else {
				rval = new (malloc(sizeof(CBuf))) CBuf();
				totl_++;
				
			}
			return rval;
		}

		void put(void *p)
		{
			if ( p ) {
				CMtx::lg guard( &mtx_ );
				void **nextp = reinterpret_cast<void**>( p );
				*nextp = h_;
				h_ = p;
				free_++;
			}
		}

		void dump()
		{
			printf( "Buf: Total %u, Free %u\n", totl_, free_ );
		}

		static FL & theFL()
		{
		static FL theFL_;
			return theFL_;
		}

		class D {
		public:
			void operator()(void *p)
			{
				theFL().put(p);
			}
		};
	};

public:

	virtual uint8_t* getPayload()  { return dat_ + off_; }
	virtual size_t   getSize()     { return len_; }
	virtual size_t   getCapacity() { return sizeof(dat_); }

	virtual unsigned getLen()      { return chl_;         }

	virtual Buf createAtHead(size_t s, bool clip)
	{
		if ( chl_ == 1 )
			throw InternalError("Chains > 1 not implemented");
		chl_++;
		return Buf(self_);
	}

	virtual Buf createAtTail(size_t s, bool clip)
	{
		return createAtHead(s, clip);
	}

	virtual void     setPayload(uint8_t *p)
	{
	unsigned end = off_ + len_;

		if ( !p )
			p = dat_;
		if ( p < dat_ || (off_ = p-dat_) > sizeof(dat_) ) {
			throw InternalError("Buf payload out of bounds");
		}
		if ( end < off_ )
			len_ = 0;
		else
			len_ = end - off_;
	}

	virtual void setSize(size_t   len)
	{
		if ( off_ + len > sizeof(dat_) )
			len_ = sizeof(dat_) - len;
		else
			len_ = len;
	}

	virtual ~CBuf ()
	{
	}

	virtual Buf getHead()
	{
		if ( chl_ == 1 )
			return Buf(self_);
		return Buf();
	}

	virtual Buf getTail()
	{
		return getHead();
	}

	static void dumpStats()
	{
		FL::theFL().dump();
	}

	class BadSize {};

	virtual uint64_t extract(void *dst, uint64_t off, uint64_t size)
	{
		if ( off + size > getSize() )
			throw BadSize();

		memcpy( dst, getPayload() + off, size );

		return size;
	}

	virtual void insert(void *src, uint64_t off, uint64_t size)
	{
		if ( off_ + off + size > getCapacity() )
			throw BadSize();
		if ( off + size > getSize() )
			setSize(off + size);
		memcpy( getPayload() + off, src, size );
	}

	// unimplemented stuff
	virtual BufChain yield_ownership()   { throw InternalError("Not Implemented"); }
	virtual void     addAtHead(Buf)      { throw InternalError("Not Implemented"); }
	virtual void     addAtTail(Buf)      { throw InternalError("Not Implemented"); }
	virtual size_t   getAvail()          { throw InternalError("Not Implemented"); }
	virtual size_t   getHeadroom()       { return off_;                            }
	virtual bool     adjPayload(ssize_t) { throw InternalError("Not Implemented"); }
	virtual void     reinit()            { throw InternalError("Not Implemented"); }
	virtual BufChain getChain()          { throw InternalError("Not Implemented"); }
	virtual void     after(Buf)          { throw InternalError("Not Implemented"); }
	virtual void     before(Buf)         { throw InternalError("Not Implemented"); }
	virtual void     unlink()            { throw InternalError("Not Implemented"); }
	virtual void     split()             { throw InternalError("Not Implemented"); }

	virtual Buf      getNext()           { return Buf(); }
	virtual Buf      getPrev()           { return Buf(); }

	static BufChain createChain();
	static Buf      createBuf();
};

Buf CBuf::createBuf()
{
CBuf *p =FL::theFL().get();
shared_ptr<CBuf> rval( p, FL::D() );
	p->self_ = rval;
	return rval;
}

BufChain CBuf::createChain()
{
CBuf *p =FL::theFL().get();
shared_ptr<CBuf> rval( p, FL::D() );
	p->self_ = rval;
	return rval;
}


BufChain IBufChain::create()
{
	return CBuf::createChain();
}


class CQueue : public IBufQueue {
private:
	unsigned depth_;
	unsigned wwater_;
	unsigned rp_, wp_;
	EventBufSync  rdSync_, wrSync_; 
	bool     isOpen_;
	vector<BufChain> q_;

	CMtx     mtx_;

protected:
	CQueue(unsigned depth, unsigned wwater)
	: depth_(depth),
	  wwater_(wwater),
	  rp_(0),
      wp_(0),
	  rdSync_( CEventBufSync::create(0,0) ),
	  wrSync_( CEventBufSync::create(depth, wwater) ),
	  isOpen_(true),
	  q_(depth),
	  mtx_("QUEUE")
	{
		printf("Q %p: rdSync %p wrSync %p\n", this, rdSync_.get(), wrSync_.get());
	}

	virtual bool push(BufChain b, bool wait, const CTimeout *abs_timeout)
	{
		if ( wrSync_->getSlot( wait, abs_timeout ) ) {
			{
			CMtx::lg guard( &mtx_ );
			q_[wp_] = b;
			if ( ++wp_ == depth_ )
				wp_ = 0;
			}
			rdSync_->putSlot();
			return true;
		}
		return false;
	}

	virtual BufChain pop(bool wait, const CTimeout *abs_timeout)
	{
	BufChain rval;
	
		if ( rdSync_->getSlot(wait, abs_timeout) ) {
			{
			CMtx::lg guard( &mtx_ );
			rval.swap( q_[rp_] );
			if ( ++rp_ == depth_ )
				rp_ = 0;
			}
			wrSync_->putSlot();
		}
if ( wait && !rval )
	throw InternalError("???");
			
		return rval;
	}


public:
	virtual bool isFull()
	{
		return wrSync_->getAvailSlots() <= wwater_;
	}

	virtual bool isEmpty()
	{
		return rdSync_->getAvailSlots() <= 0;
	}

	virtual BufChain  pop(const CTimeout *abs_timeout)
	{
		return pop(true, abs_timeout);
	}

	virtual BufChain  tryPop()
	{
		return pop(false, NULL);
	}

	virtual bool push(BufChain b, const CTimeout *abs_timeout)
	{
		return push(b, true, abs_timeout);
	}

	virtual bool tryPush(BufChain b)
	{
		return push(b, false, NULL);
	}


	virtual void shutdown()
	{
	unsigned i;

		if ( ! isOpen_ )
			return;

		isOpen_ = false;

		i = 0;
		while ( i<depth_ ) {
			if ( rdSync_->getSlot( false, 0 ) )
				i++;
			if ( wrSync_->getSlot( false, 0 ) )
				i++;
		}
		rp_ = wp_ = 0;

	}

	virtual void startup()
	{
	unsigned i;
		if ( isOpen_ )
			return;

		rp_ = wp_ = 0;

		isOpen_ = true;

		for ( i = 0; i<depth_; i++ )
			wrSync_->putSlot();
		
	}

	virtual ~CQueue()
	{
	// drain the queue
		while ( pop(false, 0) )
			;
	}

	virtual IEventSource *getReadEventSource()  { return rdSync_->getEventSource(); }
	virtual IEventSource *getWriteEventSource() { return wrSync_->getEventSource(); }

	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout)
	{
		return rdSync_->getAbsTimeout( rel_timeout );
	}

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)
	{
		return wrSync_->getAbsTimeout( rel_timeout );
	}

	static BufQueue create(unsigned depth, unsigned wwater = 0);
};

BufQueue CQueue::create(unsigned depth, unsigned wwater)
{
CQueue *p = new CQueue(depth, wwater);
	return BufQueue(p);
}

	
BufQueue IBufQueue::create(unsigned depth)
{
	return CQueue::create(depth, 0);
}

bool CEventBufSync::getSlot(bool wait, const CTimeout *abs_timeout)
{
	do {
		if ( 0 != slots_.fetch_sub(1, memory_order_acquire) ) {
			return true;
		}

		// restore

		// if someone posts right here then 'slots' is
		// -1 and putSlot() may fail to notify...

		if ( water_ == slots_.fetch_add(1, memory_order_release) ) {
			// if we're going to wait then 'processEvent'
			// will poll immediately anyways
			if ( ! wait )
				notify();
		}

	} while ( wait && evSet_->processEvent(wait, abs_timeout) );

	return false;
}

void CEventBufSync::putSlot()
{
int was;
	if ( water_ == ( was = slots_.fetch_add(1, memory_order_release) ) ) {
		notify();
	}
}

EventBufSync CEventBufSync::create(unsigned depth, unsigned water)
{
CEventBufSync *p = new CEventBufSync(depth, water);
	return EventBufSync(p);
}

class CPort : public IProtoPort {
private:
	unsigned depth_;

	BufQueue    q_;

protected:
	BufQueue getQ() { return q_; }

public:
	CPort(unsigned depth)
	: depth_(depth)
	{
		q_ = IBufQueue::create( depth_ );
	}

	virtual ProtoPort getUpstreamPort()
	{
		return ProtoPort();
	}

	virtual IEventSource *getReadEventSource()
	{
		return q_->getReadEventSource();
	}

	virtual BufChain pop(const CTimeout *to)
	{
		return q_->pop(to);
	}

	virtual BufChain tryPop()
	{
		return q_->tryPop();
	}

	virtual void start()
	{
	}

	virtual void stop()
	{
	}

	virtual void handle(IIntEventSource *event_source)
	{
	// dont' have to do anything
	}


	virtual void attach(ProtoPort upstream)                         = 0;
	virtual bool push(BufChain, const CTimeout *)                   = 0;
	virtual bool tryPush(BufChain)                                  = 0;
};

// wrappers so we can independently implement 'push' and 'tryPush()'
class CPortA : public CPort {
public:
	CPortA(unsigned depth)
	: CPort(depth)	
	{
	}

	virtual bool pushA(BufChain, bool, const CTimeout*) = 0;

	virtual bool push(BufChain b, const CTimeout *to) { return pushA(b, true, to); }

	virtual bool tryPush(BufChain b) { return pushA(b, false, NULL); }
};

class CPortB : public CPort {
public:
	CPortB(unsigned depth)
	: CPort(depth)	
	{
	}

	virtual bool pushB(BufChain, bool, const CTimeout *) = 0;

	virtual bool push(BufChain b, const CTimeout *to) { return pushB(b, true, to); }

	virtual bool tryPush(BufChain b) { return pushB(b, false, NULL); }
};

class CLoopbackPorts : public ILoopbackPorts, public CPortA, public CPortB {
private:
	weak_ptr<CLoopbackPorts> self_;
	unsigned lossPercent_;
	vector<BufChain> garblA_;
	vector<BufChain> garblB_;
	unsigned    garbl_;
public:
	CLoopbackPorts(unsigned depth, unsigned lossPercent, unsigned garbl)
	: CPortA(depth),
	  CPortB(depth),
	  lossPercent_(lossPercent),
	  garblA_(vector<BufChain>(garbl)),
	  garblB_(vector<BufChain>(garbl)),
	  garbl_(garbl)
	{}

	virtual bool pushA(BufChain, bool, const CTimeout *);
	virtual bool pushB(BufChain, bool, const CTimeout *);

	virtual void attach(ProtoPort);

	virtual ProtoPort getPortA() { return static_pointer_cast<CPortA>( shared_ptr<CLoopbackPorts>( self_ ) ); }
	virtual ProtoPort getPortB() { return static_pointer_cast<CPortB>( shared_ptr<CLoopbackPorts>( self_ ) ); }

	static BufChain dup(BufChain b);

	static LoopbackPorts create(unsigned depth, unsigned lossPercent, unsigned garbl);
};

bool CLoopbackPorts::pushA(BufChain b, bool wait, const CTimeout *to)
{
	if ( (lrand48() % 100) >= (long)lossPercent_ ) {
		BufChain db = dup( b );

		if (garbl_) {
			garblA_[ lrand48() % garbl_ ].swap( db );
		}

	// duplicate buffer here since the receiver is supposed to own 'their' copy
		return db ? (wait ? CPortB::getQ()->push( db, to) : CPortB::getQ()->tryPush(db)) : true;
	} else {
		return true;
	}
}

bool CLoopbackPorts::pushB(BufChain b, bool wait, const CTimeout *to)
{
	if ( (lrand48() % 100) >= (long)lossPercent_ ) {
		BufChain db = dup( b );

		if (garbl_) {
			garblB_[ lrand48() % garbl_ ].swap( db );
		}

	// duplicate buffer here since the receiver is supposed to own 'their' copy
		return db ? (wait ? CPortA::getQ()->push( db, to) : CPortA::getQ()->tryPush(db)) : true;
	} else {
		return true;
	}
}


BufChain CLoopbackPorts::dup(BufChain bc)
{
BufChain    rval = IBufChain::create();
Buf         rb   = rval->createAtHead( bc->getSize() );       
Buf         b    = bc->getHead();
uint8_t *old_pld = b->getPayload();
unsigned     off;

	b->setPayload( NULL );
	off = old_pld - b->getPayload();
	b->setPayload(old_pld);

	rb->setPayload( rb->getPayload() + off );
	rb->setSize( b->getSize() );
	memcpy(rb->getPayload(), b->getPayload(), b->getSize());

	return rval;
}

void CLoopbackPorts::attach(ProtoPort p)
{
	// there can never be an upstream port
	throw InternalError("Loopback port can never be attached");
}


LoopbackPorts CLoopbackPorts::create(unsigned depth, unsigned lossPercent, unsigned garbl)
{
CLoopbackPorts *p = new CLoopbackPorts(depth, lossPercent, garbl);

shared_ptr<CLoopbackPorts> rval(p);

	p->self_ =  rval;
	return rval;
}

LoopbackPorts ILoopbackPorts::create(unsigned depth, unsigned lossPercent, unsigned garbl)
{
	return CLoopbackPorts::create(depth, lossPercent, garbl);
}

class CSink : public IProtoPort, public CRunnable {
private:
	ProtoPort        upstream_;
	const char *name_;
public:
	CSink(const char *name) : CRunnable(name), name_(name)        {}

	virtual ProtoPort getUpstreamPort() { return upstream_; }

	virtual BufChain pop(const CTimeout *to)          { throw InternalError("cannot pop from sink"); }
	virtual BufChain tryPop()                         { throw InternalError("cannot pop from sink"); }

	virtual bool checkForReadEvent() { return false; }

	virtual void attach(ProtoPort upstream)           { upstream_ = upstream;                    }
	virtual bool push(BufChain b, const CTimeout *to) { return upstream_->push( b, to );         }
	virtual bool tryPush(BufChain b)                  { return upstream_->tryPush( b );          }

	virtual IEventSource *getReadEventSource()        { return NULL;                             }

	virtual void *threadBody()
	{
	BufChain  bc;
	unsigned i,j,k;
		bc = upstream_->pop( NULL );
		memcpy( &j, bc->getHead()->getPayload(), sizeof(i) );
		while ( 1 ) {
			k = 0;
			while ( k++ < 20) {
				bc = upstream_->pop( NULL );
				memcpy( &i, bc->getHead()->getPayload(), sizeof(i) );
				if ( i != j+1 ) {
					fprintf(stderr,"Sink: mismatch @i: %u, j+1: %u\n", i, j+1);
					throw InternalError("Sink got bad sequence number");
				}
				j = i;
			}
#if 0
	struct timespec wai;
			wai.tv_sec  = 0;
			wai.tv_nsec = 1000000;
			if ( nanosleep(&wai, NULL) ) {
				throw InternalError("nanosleep failed");
			}
#endif
		}
		return NULL;
	}

	virtual void start() { threadStart(); }
	virtual void stop()  { threadStop();  }

	virtual ~CSink()
	{
		threadStop();
	}

};

ProtoPort ISink::create(const char *name)
{
CSink *s = new CSink(name);
	ProtoPort rval = ProtoPort(s);
	return rval;
}

class SD {
private:
	int sd_;
public:
	SD()
	{
		if ( (sd_ = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
			throw InternalError("Unable to create socket");
	}

	int get() { return sd_; }

	~SD()
	{
		close(sd_);
	}
};

class CUdpPort : public IUdpPort, public CRunnable {
private:
	SD                 sd_;
	struct sockaddr_in peer_;
	BufQueue           outQ_;
	BufQueue           inpQ_;

public:
	CUdpPort(const char *ina, unsigned port)
	: CRunnable("UDP RX"),
	  outQ_( IBufQueue::create(4) )
	{
	struct sockaddr_in sin;

		sin.sin_family      = AF_INET;
		sin.sin_addr.s_addr = ina ? inet_addr(ina) : INADDR_ANY;
		sin.sin_port        = htons( (short)port );

		peer_.sin_port      = htons(0);

		if ( bind(sd_.get(), (struct sockaddr*) &sin, sizeof(sin)) < 0 ) 
			throw InternalError("Unable to bind",errno);
	}

	virtual ProtoPort getUpstreamPort()
	{
		return ProtoPort();
	}

	virtual BufChain pop(const CTimeout *to)
	{
		return outQ_->pop( to );
	}

	unsigned isConnected()
	{
		return ntohs( peer_.sin_port );
	}

	virtual BufChain tryPop()
	{
		return outQ_->tryPop();
	}

	virtual IEventSource *getReadEventSource()
	{
		return outQ_->getReadEventSource();
	}

	virtual void attach(ProtoPort upstream)
	{
		throw InternalError("This must be a 'top' port");
	}

	virtual bool doPush(BufChain bc, bool wait, const CTimeout *to)
	{
	int put;
	int flgs = wait ? 0 : MSG_DONTWAIT;

		if ( to ) 
			throw InternalError("Not implemented");

		if ( 0 == ntohs(peer_.sin_port) )
			return false;

		Buf b = bc->getHead();
		
		if ( (put = ::sendto(sd_.get(), b->getPayload(), b->getSize(), flgs, (struct sockaddr*)&peer_, sizeof(peer_))) < 0 )
			return false;

		return true;
	}

	virtual bool push(BufChain bc, const CTimeout *to)
	{
		return doPush(bc, true, to);
	}

	virtual void *threadBody()
	{
	int got;

		while ( 1 ) {

			BufChain bc = IBufChain::create();
			Buf      b  = bc->createAtHead( IBuf::CAPA_ETH_BIG );

			b->setPayload( NULL );

			socklen_t sz = sizeof(peer_);

			if ( (got = ::recvfrom(sd_.get(), b->getPayload(), b->getCapacity(), 0, (struct sockaddr*)&peer_, &sz)) < 0 )
				throw InternalError("recvfrom failed", errno);

			b->setSize( got );

			outQ_->tryPush( bc );
		}

		return NULL;
	}

	virtual bool tryPush(BufChain bc)
	{
		return doPush(bc, false, NULL);
	}

	virtual void start()
	{
		threadStart();
	}

	virtual void stop()
	{
		threadStop();
	}
};

UdpPort IUdpPort::create(const char *ina, unsigned port)
{
CUdpPort *p = new CUdpPort(ina, port);
	return UdpPort(p);
}
