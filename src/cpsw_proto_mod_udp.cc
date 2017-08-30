 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <cpsw_proto_mod_udp.h>

#include <errno.h>
#include <sys/select.h>

#include <stdio.h>

#include <cpsw_yaml.h>

//#define UDP_DEBUG
//#define UDP_DEBUG_STRM

CUdpHandlerThread::CUdpHandlerThread(
	const char         *name,
	int                 threadPriority,
	struct sockaddr_in *dest,
	struct sockaddr_in *me_p
)
: CRunnable(name, threadPriority)
{
	sd_.init( dest, me_p, false );
}

CUdpHandlerThread::CUdpHandlerThread(CUdpHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me_p)
: CRunnable(orig),
  sd_(orig.sd_)
{
	sd_.init( dest, me_p, false );
}

#define NBUFS_MAX 8

void * CProtoModUdp::CUdpRxHandlerThread::threadBody()
{
	ssize_t          got,siz,cap;
	std::vector<Buf> bufs;
	unsigned         idx;

	struct iovec     iov[NBUFS_MAX];
	int              niovs;

	for ( niovs = 0, cap = 0; niovs<NBUFS_MAX && cap < (ssize_t)IBuf::CAPA_ETH_JUM; niovs++ ) {
		bufs.push_back( IBuf::getBuf( IBuf::CAPA_ETH_JUM, true ) );
		iov[niovs].iov_base = bufs[niovs]->getPayload();
		cap += (iov[niovs].iov_len  = bufs[niovs]->getAvail());
	}

	while ( 1 ) {

#ifdef UDP_DEBUG
		printf("UDP -- waiting for data\n");
#endif
		got = ::readv( sd_.getSd(), iov, niovs );
		if ( got < 0 ) {
			perror("rx thread");
			sleep(10);
			continue;
		}
		nDgrams_.fetch_add(1,   boost::memory_order_relaxed);
		nOctets_.fetch_add(got, boost::memory_order_relaxed);

		if ( got > 0 ) {
			BufChain bufch = IBufChain::create();

			siz = got;
			idx = 0;
			while ( siz > 0 ) {
				if ( siz < (cap = bufs[idx]->getAvail()) ) {
					cap = siz;
				}
				bufs[idx]->setSize( cap );
#ifdef UDP_DEBUG
				if ( idx == 0 ) {
					int      i;
					uint8_t  *p = bufs[idx]->getPayload();
#ifdef UDP_DEBUG_STRM
					unsigned fram = (p[1]<<4) | (p[0]>>4);
					unsigned frag = (p[4]<<16) | (p[3] << 8) | p[2];
#endif
					printf("UDP data: ");
					for ( i=0; i< (got < 4 ? got : 4); i++ )
						printf("%02x ", p[i]);
					printf("\n");
				}
#endif

				bufch->addAtTail( bufs[idx] );

				// get new buffers
				bufs[idx] = IBuf::getBuf( IBuf::CAPA_ETH_HDR );
				iov[idx].iov_base = bufs[idx]->getPayload();
				iov[idx].iov_len  = bufs[idx]->getAvail();
				idx++;
				siz -= cap;
			}

#ifdef UDP_DEBUG
		bool st=
#endif
			// do NOT wait indefinitely
			// could be that the queue is full with
			// retry replies they will only discover
			// next time they care about reading from
			// this VC...
			owner_->pushDown( bufch, &TIMEOUT_NONE );

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

		}
#ifdef UDP_DEBUG
		else {
			printf("UDP got ZERO\n");
		}
#endif
	}
	return NULL;
}

CProtoModUdp::CUdpRxHandlerThread::CUdpRxHandlerThread(
	const char         *name,
	int                 threadPriority,
	struct sockaddr_in *dest,
	struct sockaddr_in *me,
	CProtoModUdp       *owner
)
: CUdpHandlerThread(name, threadPriority, dest, me),
  nOctets_(0),
  nDgrams_(0),
  owner_(owner)
{
}

CProtoModUdp::CUdpRxHandlerThread::CUdpRxHandlerThread(CUdpRxHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me, CProtoModUdp *owner)
: CUdpHandlerThread( orig, dest, me ),
  nOctets_(0),
  nDgrams_(0),
  owner_(owner)
{
}

void * CUdpPeerPollerThread::threadBody()
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
	return NULL;
}

CUdpPeerPollerThread::CUdpPeerPollerThread(const char *name, struct sockaddr_in *dest, struct sockaddr_in *me, unsigned pollSecs)
: CUdpHandlerThread(name, IProtoStackBuilder::NORT_THREAD_PRIORITY, dest, me),
  pollSecs_(pollSecs)
{
}

CUdpPeerPollerThread::CUdpPeerPollerThread(CUdpPeerPollerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me)
: CUdpHandlerThread(orig, dest, me),
  pollSecs_(orig.pollSecs_)
{
}

void CProtoModUdp::createThreads(unsigned nRxThreads, int pollSeconds)
{
	unsigned i;
	struct sockaddr_in me;

	tx_.getMyAddr( &me );

	if ( poller_ ) {
		// called from copy constructor
		poller_ = new CUdpPeerPollerThread(*poller_, &dest_, &me);
	} else if ( pollSeconds > 0 ) {
		poller_ = new CUdpPeerPollerThread("UDP Poller (UDP protocol module)", &dest_, &me, pollSeconds );
	}

	// might be called by the copy constructor
	rxHandlers_.clear();

	for ( i=0; i<nRxThreads; i++ ) {
		rxHandlers_.push_back( new CUdpRxHandlerThread("UDP RX Handler (UDP protocol module)", threadPriority_, &dest_, &me, this ) );
	}

	// maybe setting the threadPriority failed?
	if ( nRxThreads ) {
		threadPriority_ = rxHandlers_[0]->getPrio();
	}
}

void CProtoModUdp::modStartup()
{
unsigned i;
	if ( poller_ )
		poller_->threadStart();
	for ( i=0; i<rxHandlers_.size(); i++ ) {
		rxHandlers_[i]->threadStart();
	}
}

void CProtoModUdp::modShutdown()
{
unsigned i;
	if ( poller_ )
		poller_->threadStop();

	for ( i=0; i<rxHandlers_.size(); i++ ) {
		rxHandlers_[i]->threadStop();
	}
}

CProtoModUdp::CProtoModUdp(
	Key                &k,
	struct sockaddr_in *dest,
	unsigned            depth,
	int                 threadPriority,
	unsigned            nRxThreads,
	int                 pollSecs
)
:CProtoMod(k, depth),
 dest_(*dest),
 nTxOctets_(0),
 nTxDgrams_(0),
 threadPriority_(threadPriority),
 poller_( NULL )
{
	tx_.init( dest, 0, true );
	createThreads( nRxThreads, pollSecs );
}

void
CProtoModUdp::dumpYaml(YAML::Node &node) const
{
	YAML::Node udpParms;
	writeNode(udpParms, YAML_KEY_port,          getDestPort()     );
	writeNode(udpParms, YAML_KEY_outQueueDepth, getQueueDepth()   );
	writeNode(udpParms, YAML_KEY_numRxThreads,  rxHandlers_.size());
	writeNode(udpParms, YAML_KEY_pollSecs,      poller_ ? poller_->getPollSecs() : 0);
	if ( threadPriority_ != IProtoStackBuilder::DFLT_THREAD_PRIORITY ) {
		writeNode(udpParms, YAML_KEY_threadPriority,  threadPriority_);
	}
	writeNode(node, YAML_KEY_UDP, udpParms);
}

CProtoModUdp::CProtoModUdp(CProtoModUdp &orig, Key &k)
:CProtoMod(orig, k),
 dest_(orig.dest_),
 tx_(orig.tx_),
 nTxOctets_(0),
 nTxDgrams_(0),
 threadPriority_(orig.threadPriority_),
 poller_(orig.poller_)
{
	tx_.init( &dest_, 0, true );
	createThreads( orig.rxHandlers_.size(), -1 );
}

uint64_t CProtoModUdp::getNumRxOctets()
{
unsigned i;
uint64_t rval = 0;

	for ( i=0; i<rxHandlers_.size(); i++ )
		rval += rxHandlers_[i]->getNumOctets();
	return rval;
}

uint64_t CProtoModUdp::getNumRxDgrams()
{
unsigned i;
uint64_t rval = 0;

	for ( i=0; i<rxHandlers_.size(); i++ )
		rval += rxHandlers_[i]->getNumDgrams();
	return rval;
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
	fprintf(f,"  Peer port : %15u\n",    getDestPort());
	fprintf(f,"  RX Threads: %15lu\n",   rxHandlers_.size());
	fprintf(f,"  ThreadPrio: %15d\n",    threadPriority_);
	fprintf(f,"  Has Poller:               %c\n", poller_ ? 'Y' : 'N');
	fprintf(f,"  #TX Octets: %15"PRIu64"\n", getNumTxOctets());
	fprintf(f,"  #TX DGRAMs: %15"PRIu64"\n", getNumTxDgrams());
	fprintf(f,"  #RX Octets: %15"PRIu64"\n", getNumRxOctets());
	fprintf(f,"  #RX DGRAMs: %15"PRIu64"\n", getNumRxDgrams());
}

bool CProtoModUdp::doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout)
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
	// If they were to fragment a large frame they have to push each
	// fragment individually.

	nTxDgrams_.fetch_add( 1, boost::memory_order_relaxed );
	nTxOctets_.fetch_add( bc->getSize(), boost::memory_order_relaxed );

	for (nios=0, b=bc->getHead(); nios<bc->getLen(); nios++, b=b->getNext()) {
		iov[nios].iov_base = b->getPayload();
		iov[nios].iov_len  = b->getSize();
	}

	if ( wait ) {
		FD_ZERO( &fds );

		FD_SET( tx_.getSd(), &fds );

		// use pselect: does't modify the timeout and it's a timespec
		selres = ::pselect( tx_.getSd() + 1, NULL, &fds, NULL, !timeout || timeout->isIndefinite() ? NULL : &timeout->tv_, NULL );
		if ( selres < 0  ) {
			perror("::pselect() - dropping message due to error");
			return false;
		}
		if ( selres == 0 ) {
#ifdef UDP_DEBUG
			printf("UDP doPush -- pselect timeout\n");
#endif
			// TIMEOUT
			return false;
		}
	}

	sndres = writev( tx_.getSd(), iov, nios );

	if ( sndres < 0 ) {
		perror("::writev() - dropping message due to error");
#ifdef UDP_DEBUG
// this could help debugging the occasinal EPERM I get here...
#warning FIXME
abort();
#endif
		return false;
	}

#ifdef UDP_DEBUG
	printf("UDP doPush -- wrote %d:", sndres);
	for ( unsigned i=0; i < (iov[0].iov_len < 4 ? iov[0].iov_len : 4); i++ )
		printf(" %02x", ((unsigned char*)iov[0].iov_base)[i]);
	printf("\n");
#endif
	return true;
}

int CProtoModUdp::iMatch(ProtoPortMatchParams *cmp)
{
	cmp->udpDestPort_.handledBy_ = getProtoMod();
	if ( cmp->udpDestPort_ == getDestPort() ) {
		cmp->udpDestPort_.matchedBy_ = getSelfAs<ProtoModUdp>();
		return 1;
	}
	return 0;
}
