#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <cpsw_proto_mod_udp.h>

#include <errno.h>
#include <sys/select.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sched.h>

#define UDP_DEBUG
//#define UDP_DEBUG_STRM

CSockSd::CSockSd()
{
	if ( ( sd_ = ::socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
}

void CSockSd::getMyAddr(struct sockaddr_in *addr_p)
{
	socklen_t l = sizeof(*addr_p);
	if ( getsockname( getSd(), (struct sockaddr*)addr_p, &l ) ) {
		throw IOError("getsockname() ", errno);
	}
}

CSockSd::CSockSd(CSockSd &orig)
{
	if ( ( sd_ = ::socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
		throw InternalError("Unable to create socket");
	}
}

CSockSd::~CSockSd()
{
	close( sd_ );
}

void * CUdpHandlerThread::threadBody(void *arg)
{
	CUdpHandlerThread *obj = static_cast<CUdpHandlerThread *>(arg);
	try {
		obj->threadBody();
	} catch ( CPSWError e ) {
		fprintf(stderr,"CPSW Error (CUdpHandlerThread): %s\n", e.getInfo().c_str());
		throw;
	}
	return 0;
}

static void sockIni(int sd, struct sockaddr_in *dest, struct sockaddr_in *me_p, bool nblk)
{
	int    optval = 1;

	struct sockaddr_in me;

	if ( NULL == me_p ) {
		me.sin_family      = AF_INET;
		me.sin_addr.s_addr = INADDR_ANY;
		me.sin_port        = htons( 0 );

		me_p = &me;
	}

	if ( nblk ) {
		if ( ::fcntl( sd, F_SETFL, O_NONBLOCK ) ) {
			throw IOError("fcntl(O_NONBLOCK) ", errno);
		}
	}

	if ( ::setsockopt(  sd,  SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval) ) ) {
		throw IOError("setsockopt(SO_REUSEADDR) ", errno);
	}

	if ( ::bind( sd, (struct sockaddr*)me_p, sizeof(*me_p)) ) {
		throw IOError("bind failed ", errno);
	}

	// connect - filters any traffic from other destinations/fpgas in the kernel
	if ( ::connect( sd, (struct sockaddr*)dest, sizeof(*dest) ) )
		throw IOError("connect failed ", errno);
}

CUdpHandlerThread::CUdpHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me_p)
: running_(false)
{
	sockIni( sd_.getSd(), dest, me_p, false );
}

CUdpHandlerThread::CUdpHandlerThread(CUdpHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me_p)
: running_(false)
{
	sockIni( sd_.getSd(), dest, me_p, false );
}

// only start after object is fully constructed
void CUdpHandlerThread::start()
{
	if ( pthread_create( &tid_, NULL, threadBody, this ) )
		throw IOError("unable to create thread ", errno);
	running_ = true;
}

CUdpHandlerThread::~CUdpHandlerThread()
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

void CProtoModUdp::CUdpRxHandlerThread::threadBody()
{
	ssize_t got;
	Buf     buf = IBuf::getBuf();

	while ( 1 ) {

#ifdef UDP_DEBUG
		printf("UDP -- waiting for data\n");
#endif
		got = ::read( sd_.getSd(), buf->getPayload(), buf->getCapacity() );
		if ( got < 0 ) {
			perror("rx thread");
			sleep(10);
			continue;
		}
		buf->setSize( got );
		if ( got > 0 ) {

#ifdef UDP_DEBUG
		unsigned i;
#ifdef UDP_DEBUG_STRM
		uint8_t  *p = buf->getPayload();
		unsigned fram = (p[1]<<4) | (p[0]>>4);
		unsigned frag = (p[4]<<16) | (p[3] << 8) | p[2];
#endif
			printf("UDP data: ");
			for ( i=0; i< (got < 4 ? got : 4); i++ )
				printf("%02x ", buf->getPayload()[i]);
			printf("\n");
#endif

			BufChain bufch = IBufChain::create();
			bufch->addAtTail( buf );

#ifdef UDP_DEBUG
		bool st=
#endif
			owner_->pushDown( bufch );

#ifdef UDP_DEBUG
			printf("UDP got %d", (int)got);
#ifdef UDP_DEBUG_STRM
			printf(" fram # %4d, frag # %4d", fram, frag);
#endif
			if ( st )
				printf(" (pushdown SUCC)\n");
			else
				printf(" (pushdown DROP)\n");
#endif

			// get new buffer
			buf = IBuf::getBuf();
		}
#ifdef UDP_DEBUG
		else {
			printf("UDP got ZERO\n");
		}
#endif
	}
}

CProtoModUdp::CUdpRxHandlerThread::CUdpRxHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me, CProtoModUdp *owner)
: CUdpHandlerThread(dest, me),
  owner_( owner )
{
}

CProtoModUdp::CUdpRxHandlerThread::CUdpRxHandlerThread(CUdpRxHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me, CProtoModUdp *owner)
: CUdpHandlerThread( orig, dest, me),
  owner_( owner )
{
}

void CUdpPeerPollerThread::threadBody()
{
	uint8_t buf[4];
	memset( buf, 0, sizeof(buf) );
	while ( 1 ) {
		if ( ::write( sd_.getSd(), buf, 0 ) < 0 ) {
			perror("poller thread (write)");
			continue;
		}
		if ( sleep( pollSecs_ ) )
			continue; // interrupted by signal
	}
}

CUdpPeerPollerThread::CUdpPeerPollerThread(struct sockaddr_in *dest, struct sockaddr_in *me, unsigned pollSecs)
: CUdpHandlerThread(dest, me),
  pollSecs_(pollSecs)
{
}

CUdpPeerPollerThread::CUdpPeerPollerThread(CUdpPeerPollerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me)
: CUdpHandlerThread(orig, dest, me),
  pollSecs_(orig.pollSecs_)
{
}

void CProtoModUdp::spawnThreads(unsigned nRxThreads, int pollSeconds)
{
	unsigned i;
	struct sockaddr_in me;

	tx_.getMyAddr( &me );

	if ( poller_ ) {
		// called from copy constructor
		poller_ = new CUdpPeerPollerThread( *poller_, &dest_, &me);
	} else if ( pollSeconds > 0 ) {
		poller_ = new CUdpPeerPollerThread( &dest_, &me, pollSeconds );
	}

	// might be called by the copy constructor
	rxHandlers_.clear();

	for ( i=0; i<nRxThreads; i++ ) {
		rxHandlers_.push_back( new CUdpRxHandlerThread( &dest_, &me, this ) );
	}

	if ( poller_ )
		poller_->start();
	for ( i=0; i<rxHandlers_.size(); i++ ) {
		rxHandlers_[i]->start();
	}
}

CProtoModUdp::CProtoModUdp(Key &k, struct sockaddr_in *dest, CBufQueueBase::size_type depth, unsigned nRxThreads, int pollSecs)
:CProtoMod(k, depth),
 dest_(*dest),
 poller_( NULL )
{
	sockIni( tx_.getSd(), dest, 0, true );
	spawnThreads( nRxThreads, pollSecs );
}

CProtoModUdp::CProtoModUdp(CProtoModUdp &orig, Key &k)
:CProtoMod(orig, k),
 dest_(orig.dest_),
 poller_(orig.poller_)
{
	sockIni( tx_.getSd(), &dest_, 0, true );
	spawnThreads( orig.rxHandlers_.size(), -1 );
}

CProtoModUdp::~CProtoModUdp()
{
unsigned i;
	for ( i=0; i<rxHandlers_.size(); i++ )
		delete rxHandlers_[i];
	if ( poller_ )
		delete poller_;
}

void CProtoModUdp::dumpInfo(FILE *f)
{
	if ( ! f )
		f = stdout;

	fprintf(f,"CProtoModUdp:\n");
}

void CProtoModUdp::doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout)
{
fd_set         fds;
int            selres, sndres;
Buf            b;
struct iovec   iov[bc->getLen()];
unsigned       nios;

	// there could be two models for sending a chain of buffers:
	// a) the chain describes a gather list (one UDP message assembled from chain)
	// b) the chain describes a fragmented frame (multiple messages sent)
	// We follow a) here...
	// If they were to frament a large frame they have to push each
	// fragment individually.

	for (nios=0, b=bc->getHead(); nios<bc->getLen(); nios++, b=b->getNext()) {
		iov[nios].iov_base = b->getPayload();
		iov[nios].iov_len  = b->getSize();
	}

	if ( wait ) {
		FD_ZERO( &fds );

		FD_SET( tx_.getSd(), &fds );

		// use pselect: does't modify the timeout and it's a timespec
		selres = ::pselect( tx_.getSd() + 1, NULL, &fds, NULL, &timeout->tv_, NULL );
		if ( selres < 0  ) {
			perror("::pselect() - dropping message due to error");
			return;
		}
		if ( selres == 0 ) {
#ifdef UDP_DEBUG
			printf("UDP doPush -- pselect timeout\n");
#endif
			// TIMEOUT
			return;
		}
	}

	sndres = writev( tx_.getSd(), iov, nios );

	if ( sndres < 0 ) {
		perror("::writev() - dropping message due to error");
		return;
	}

#ifdef UDP_DEBUG
	printf("UDP doPush -- wrote %d:", sndres);
	for ( unsigned i=0; i < (iov[0].iov_len < 4 ? iov[0].iov_len : 4); i++ )
		printf(" %02x", ((unsigned char*)iov[0].iov_base)[i]);
	printf("\n");
#endif
}

int CProtoModUdp::iMatch(ProtoPortMatchParams *cmp)
{
	if ( cmp->udpDestPort_.doMatch_ && cmp->udpDestPort_.val_ == getDestPort() ) {
		cmp->udpDestPort_.matchedBy_ = getSelfAs<ProtoModUdp>();
		return 1;
	}
	return 0;
}
