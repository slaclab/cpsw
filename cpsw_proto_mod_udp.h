#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
using boost::shared_ptr;
using boost::make_shared;

class CSockSd;
typedef shared_ptr<CSockSd> SockSd;

class CSockSd {
private:
	int sd_;
	CSockSd()
	{
		if ( ( sd_ = ::socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
			throw InternalError("Unable to create socket");
		}
	}

public:

	~CSockSd()
	{
		close( sd_ );
	}

	int getSd() const { return sd_; }

	static SockSd create()
	{
		CSockSd *s = new CSockSd();
		return SockSd(s);
	}
};

class CUdpHandlerThread {
protected:
	SockSd         sd_;
	pthread_t      tid_;
	bool           running_;

private:
	static void * threadBody(void *arg)
	{
		CUdpHandlerThread *obj = static_cast<CUdpHandlerThread *>(arg);
		obj->threadBody();
		return 0;
	}

protected:
	virtual void threadBody() = 0;

public:
	virtual void getMyAddr(struct sockaddr_in *addr_p)
	{
	socklen_t l = sizeof(*addr_p);
		if ( getsockname( sd_->getSd(), (struct sockaddr*)addr_p, &l ) ) {
			throw IOError("getsockname() ", errno);
		}
	}

	CUdpHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me_p = NULL)
	: sd_( CSockSd::create() ),
	  running_(false)
	{
	int    optval = 1;

	struct sockaddr_in me;
	
		if ( NULL == me_p ) {
			me.sin_family      = AF_INET;
			me.sin_addr.s_addr = INADDR_ANY;
			me.sin_port        = htons( 0 );
			
			me_p = &me;
		}

		if ( ::setsockopt(  sd_->getSd(),  SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) ) {
			throw IOError("setsockopt(SO_REUSEADDR) ", errno);
		}

		if ( ::bind( sd_->getSd(), (struct sockaddr*)me_p, sizeof(*me_p)) ) {
			throw IOError("bind failed ", errno);
		}

		if ( ::connect( sd_->getSd(), (struct sockaddr*)dest, sizeof(*dest) ) )
			throw IOError("connect failed ", errno);

	}

	// only start after object is fully constructed
	virtual void start()
	{
		if ( pthread_create( &tid_, NULL, threadBody, this ) )
			throw IOError("unable to create thread ", errno);
		running_ = true;
	}

	virtual ~CUdpHandlerThread()
	{
	void *rval;
		if ( running_ ) {
			if ( pthread_cancel( tid_ ) ) {
				throw IOError("pthread_cancel: ", errno);
			}
			if ( pthread_join( tid_, &rval  ) ) {
				throw IOError("pthread_join: ", errno);
			}
		}
	}

	template <typename C, typename A1>
	static shared_ptr<C> create(A1 a1)
	{
		return make_shared<C>(a1);
	}

	template <typename C, typename A1, typename A2>
	static shared_ptr<C> create(A1 a1, A1 a2)
	{
		return make_shared<C>(a1, a2);
	}

	template <typename C, typename A1, typename A2, typename A3>
	static shared_ptr<C> create(A1 a1, A2 a2, A3 a3)
	{
		return make_shared<C>(a1, a2, a3);
	}
};

class CUdpRxHandlerThread : public CUdpHandlerThread {
public:
	CBufQueue  *pOutputQueue_;

protected:
	virtual void threadBody()
	{
		ssize_t got;

		while ( 1 ) {
			Buf buf = IBuf::getBuf();
			got = ::read( sd_->getSd(), buf->getPayload(), buf->getSize() );
			if ( got < 0 ) {
				break;
			}
			buf->setSize( got );
			if ( got > 0 ) {
				BufChain bufch = IBufChain::create();
				bufch->addAtHead( buf );
				pOutputQueue_->push( &bufch );
			}
		}
	}

public:
	CUdpRxHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me, CBufQueue *oqueue)
	: CUdpHandlerThread(dest, me),
	  pOutputQueue_( oqueue )
	{
	}
};

class CUdpPeerPollerThread : public CUdpHandlerThread {
private:
	unsigned pollSecs_;

protected:
	virtual void threadBody()
	{
	uint8_t buf[4];
		memset( buf, 0, sizeof(buf) );
		while ( 1 ) {
			if ( ::write( sd_->getSd(), buf, sizeof(buf) ) < 0 )
				break;
			if ( sleep( pollSecs_ ) )
				break; // interrupted by signal
		}
	}
public:
	CUdpPeerPollerThread(struct sockaddr_in *dest, struct sockaddr_in *me = NULL, unsigned pollSecs = 60)
	: CUdpHandlerThread(dest, me),
	  pollSecs_(pollSecs)
	{
	}
};

class CProtoModUdp : public CProtoMod {
protected:
	std::vector< CUdpRxHandlerThread > rxHandlers_;
	CUdpPeerPollerThread               poller_;
public:
	CProtoModUdp(struct sockaddr_in *dest, CBufQueueBase::size_type depth, unsigned nThreads = 1)
	:CProtoMod(depth),
	 poller_( CUdpPeerPollerThread(dest, NULL) )
	{
	unsigned i;
	struct sockaddr_in me;

		poller_.getMyAddr( &me );

		for ( i=0; i<nThreads; i++ ) {
			rxHandlers_.push_back( CUdpRxHandlerThread( dest, &me, &outputQueue_ ) );
		}

		poller_.start();
		for ( i=0; i<rxHandlers_.size(); i++ ) {
			rxHandlers_[i].start();
		}
	}

	virtual ~CProtoModUdp()
	{
	}
};
