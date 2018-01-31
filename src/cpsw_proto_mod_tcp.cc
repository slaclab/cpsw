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

#include <cpsw_proto_mod_tcp.h>

#include <errno.h>
#include <sys/select.h>

#include <stdio.h>

#include <cpsw_yaml.h>

//#define TCP_DEBUG
//#define TCP_DEBUG_STRM

#define NBUFS_MAX 8

static void xfer(
	const char   *nm,
	ssize_t     (*op)(int, const struct iovec*, int),
	int sd,
	struct iovec *iop,
	unsigned      niovs,
	ssize_t       len)
{
ssize_t xfr;
	while ( len > 0 ) {
		xfr = op( sd, iop, niovs );
		if ( xfr <= 0 ) {
			throw InternalError( std::string("TCP: ") + std::string(nm) + std::string(" error: "), errno );
		}
		while ( (size_t)xfr >= iop->iov_len ) {
			xfr    -= iop->iov_len;
			len    -= iop->iov_len;
			iop++;
			niovs--;
		}
		iop->iov_len -= xfr;
		iop->iov_base = reinterpret_cast<void*>( reinterpret_cast<uintptr_t>( iop->iov_base ) + xfr );
		len    -= xfr;
	}
}

void * CProtoModTcp::CRxHandlerThread::threadBody()
{
	ssize_t          got,siz,cap;
	std::vector<Buf> bufs;
	unsigned         idx, i;

	struct iovec     iov[NBUFS_MAX];
	unsigned         niovs;

	uint32_t         len;

	for ( niovs = 0, cap = 0; niovs<NBUFS_MAX && cap < (ssize_t)IBuf::CAPA_ETH_JUM; niovs++ ) {
		bufs.push_back( IBuf::getBuf( IBuf::CAPA_ETH_JUM, true ) );
		iov[niovs].iov_base = bufs[niovs]->getPayload();
		cap += (iov[niovs].iov_len  = bufs[niovs]->getAvail());
	}

	while ( 1 ) {
		uint8_t      *p;

#ifdef TCP_DEBUG
		printf("TCP -- waiting for data\n");
#endif

		siz = sizeof(len);
		p   = reinterpret_cast<uint8_t*>( &len );
		while ( siz > 0 ) {
			if ( (got = ::read(sd_, p, siz)) <= 0 )
				throw InternalError("TCP reading length: ", errno);
			p   += got;
			siz -= got;
		}

		len = ntohl(len);

#ifdef TCP_DEBUG
		printf("TCP RX -- got length: %"PRId32"\n", len);
#endif

		siz = len;
		idx = 0;
		while ( siz > 0 ) {
			if ( idx == niovs )
				throw InternalError("Too many TCP fragments");
			if ( (size_t)siz >= iov[idx].iov_len ) {
				siz -= iov[idx].iov_len;
			} else {
				iov[idx].iov_len = siz;
				siz = 0;
			}
			idx++;
		}
			
		xfer("readv()", ::readv, sd_, iov, idx, len);

		nDgrams_.fetch_add(1,   boost::memory_order_relaxed);
		nOctets_.fetch_add(len, boost::memory_order_relaxed);

		BufChain bufch = IBufChain::create();

		for ( i=0; i<idx; i++ ) {
			bufs[i]->setSize( iov[i].iov_len );
#ifdef TCP_DEBUG
			if ( i == 1 ) {
				uint8_t  *p = bufs[i]->getPayload();
#ifdef TCP_DEBUG_STRM
				unsigned fram = (p[1]<<4) | (p[0]>>4);
				unsigned frag = (p[4]<<16) | (p[3] << 8) | p[2];
				printf("TCP: fram %d[%d]\n", fram, frag);
#else
				int      i;
				printf("TCP got %d data: ",(int)len);
				for ( i=0; i< (len < 20 ? len : 20); i++ )
					printf("%02x ", p[i]);
				printf("\n");
#endif
			}
#endif

			bufch->addAtTail( bufs[i] );

			// get new buffers
			bufs[i] = IBuf::getBuf( IBuf::CAPA_ETH_HDR );
			iov[i].iov_base = bufs[i]->getPayload();
			iov[i].iov_len  = bufs[i]->getAvail();
		}

#ifdef TCP_DEBUG
		bool st=
#endif

			owner_->pushDown( bufch, &TIMEOUT_INDEFINITE );

#ifdef TCP_DEBUG
			if ( st )
				printf(" (pushdown SUCC)\n");
			else
				printf(" (pushdown DROP)\n");
#endif

	}
	return NULL;
}

CProtoModTcp::CRxHandlerThread::CRxHandlerThread(const char *name, int threadPriority, int sd, CProtoModTcp *owner)
: CRunnable(name, threadPriority),
  sd_(sd),
  nOctets_(0),
  nDgrams_(0),
  owner_(owner)
{
}

CProtoModTcp::CRxHandlerThread::CRxHandlerThread(CRxHandlerThread &orig, int sd, CProtoModTcp *owner)
: CRunnable(orig),
  sd_(sd),
  nOctets_(0),
  nDgrams_(0),
  owner_(owner)
{
}

void CProtoModTcp::createThread(int threadPriority)
{
	// might be called by the copy constructor
	if ( rxHandler_ ) {
		delete rxHandler_;
		rxHandler_ = NULL;
	}

	rxHandler_ = new CRxHandlerThread("TCP RX Handler (TCP protocol module)", threadPriority, sd_.getSd(), this );
}

void CProtoModTcp::modStartup()
{
	if ( rxHandler_ )
		rxHandler_->threadStart();
}

void CProtoModTcp::modShutdown()
{
	if ( rxHandler_ )
		rxHandler_->threadStop();
}

CProtoModTcp::CProtoModTcp(Key &k, struct sockaddr_in *dest, unsigned depth, int threadPriority)
:CProtoMod(k, depth),
 dest_(*dest),
 sd_(SOCK_STREAM),
 nTxOctets_(0),
 nTxDgrams_(0),
 rxHandler_(NULL)
{
	sd_.init( &dest_, 0, false );
	createThread( threadPriority );
}

void
CProtoModTcp::dumpYaml(YAML::Node &node) const
{
int prio = rxHandler_->getPrio();
	YAML::Node tcpParms;
	writeNode(tcpParms, YAML_KEY_port,          getDestPort()     );
	writeNode(tcpParms, YAML_KEY_outQueueDepth, getQueueDepth()   );
	if ( prio != IProtoStackBuilder::DFLT_THREAD_PRIORITY ) {
		writeNode(tcpParms, YAML_KEY_threadPriority, prio);
	}
	writeNode(node, YAML_KEY_TCP, tcpParms);
}

CProtoModTcp::CProtoModTcp(CProtoModTcp &orig, Key &k)
:CProtoMod(orig, k),
 dest_(orig.dest_),
 sd_(orig.sd_),
 nTxOctets_(0),
 nTxDgrams_(0)
{
	sd_.init( &dest_, 0, false );
	createThread( orig.rxHandler_->getPrio() );
}

uint64_t CProtoModTcp::getNumRxOctets()
{
	return rxHandler_ ? rxHandler_->getNumOctets() : 0;
}

uint64_t CProtoModTcp::getNumRxDgrams()
{
	return rxHandler_ ? rxHandler_->getNumDgrams() : 0;
}

CProtoModTcp::~CProtoModTcp()
{
	if ( rxHandler_ )
		delete rxHandler_;
}

void CProtoModTcp::dumpInfo(FILE *f)
{
	if ( ! f )
		f = stdout;

	fprintf(f,"CProtoModTcp:\n");
	fprintf(f,"  Peer port : %15u\n",    getDestPort());
	fprintf(f,"  ThreadPrio: %15d\n",    rxHandler_->getPrio());
	fprintf(f,"  #TX Octets: %15"PRIu64"\n", getNumTxOctets());
	fprintf(f,"  #TX DGRAMs: %15"PRIu64"\n", getNumTxDgrams());
	fprintf(f,"  #RX Octets: %15"PRIu64"\n", getNumRxOctets());
	fprintf(f,"  #RX DGRAMs: %15"PRIu64"\n", getNumRxDgrams());
}

bool CProtoModTcp::doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout)
{
Buf            b;
uint32_t       len = bc->getSize();
struct iovec   iov[ bc->getLen() ];
unsigned       nios;

	// there could be two models for sending a chain of buffers:
	// a) the chain describes a gather list (one TCP message assembled from chain)
	// b) the chain describes a fragmented frame (multiple messages sent)
	// We follow a) here...
	// If they were to fragment a large frame they have to push each
	// fragment individually.

	{
	Buf h = bc->getHead();

	uint32_t lenNBO = htonl(len);

	if ( ! h->adjPayload( - sizeof(lenNBO) ) ) {
		throw InternalError("CProtoModTcp: not enough headroom to prepend size");
	}

	memcpy( h->getPayload(), &lenNBO, sizeof(lenNBO) );

	len += sizeof(lenNBO);

	}

	nTxDgrams_.fetch_add( 1, boost::memory_order_relaxed );
	nTxOctets_.fetch_add( bc->getSize(), boost::memory_order_relaxed );

	for ( nios=0, b=bc->getHead(); b; nios++, b=b->getNext() ) {
		iov[nios].iov_base = b->getPayload();
		iov[nios].iov_len  = b->getSize();
	}

	{
	CMtx::lg GUARD( &txMtx_ );

	xfer( "writev()", ::writev, sd_.getSd(), iov, nios, len );

	}

#ifdef TCP_DEBUG
	printf("TCP doPush -- wrote %ld\n", bc->getSize());
	uint8_t dmp[40];
	bc->extract(dmp, 0, sizeof(dmp));
	for ( unsigned i=0; i<sizeof(dmp); i++ ) {
		printf("%02x ", dmp[i]);
	}
	printf("\n");
#endif
	return true;
}

int CProtoModTcp::iMatch(ProtoPortMatchParams *cmp)
{
	cmp->tcpDestPort_.handledBy_ = getProtoMod();
	if ( cmp->tcpDestPort_ == getDestPort() ) {
		cmp->tcpDestPort_.matchedBy_ = getSelfAs<ProtoModTcp>();
		return 1;
	}
	return 0;
}

unsigned CProtoModTcp::getMTU()
{
	// arbitrary
	return 65536;
}
