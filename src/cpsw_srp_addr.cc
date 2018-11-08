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

#include <cpsw_srp_addr.h>
#include <cpsw_srp_transactions.h>

#include <cpsw_proto_mod_srpmux.h>

#include <cpsw_mutex.h>

#include <cpsw_yaml.h>

using boost::dynamic_pointer_cast;

//#define SRPADDR_DEBUG 0
//#define TIMEOUT_DEBUG

// SRP (V3) does strange things when we use more than 1024
// words (2018/8); when we use a few more then a bad SRP
// status reply comes back. If we use a large number then
// the board stops responding and must be rebooted!
// UPDATE: writes officially only support 4kB max.
#define MAXTXWORDS 1024

static bool hasRssi(ProtoPort stack)
{
ProtoPortMatchParams cmp;
	cmp.haveRssi_.include();
	return cmp.requestedMatches() == cmp.findMatches( stack );
}

static bool hasDepack(ProtoPort stack)
{
ProtoPortMatchParams cmp;
	cmp.haveDepack_.include();
	return cmp.requestedMatches() == cmp.findMatches( stack );
}

CSRPAsyncHandler::CSRPAsyncHandler(AsyncIOTransactionManager xactMgr, CSRPAddressImpl *srp)
: CRunnable("SRP Async Handler"),
  xactMgr_ ( xactMgr           ),
  srp_     ( srp               )
{
}

bool
CSRPAsyncHandler::threadStop(void **join_p)
{
bool rval = CRunnable::threadStop(join_p);
	door_.reset();
	return rval;
}

void
CSRPAsyncHandler::threadStart(ProtoDoor door)
{
	door_ = door;
	CRunnable::threadStart();
}

void *
CSRPAsyncHandler::threadBody()
{
	while ( 1 ) {
		BufChain rchn = door_->pop( 0, true );

		++stats_;

		uint32_t tid_bits = srp_->extractTid( rchn );

		xactMgr_->complete( rchn, tid_bits );
	}
	return 0;
}


CSRPAddressImpl::CSRPAddressImpl(AKey key, ProtoStackBuilder bldr, ProtoPort stack)
: CCommAddressImpl( key, stack                                                                     ),
  protoVersion_   ( bldr->getSRPVersion()                                                          ),
  usrTimeout_     ( bldr->getSRPTimeoutUS()                                                        ),
  dynTimeout_     ( usrTimeout_                                                                    ),
  useDynTimeout_  ( bldr->hasSRPDynTimeout()                                                       ),
  retryCnt_       ( bldr->getSRPRetryCount() & 0xffff /* undocumented hack to test byte-resolution access */ ),
  nRetries_       ( 0                                                                              ),
  nWrites_        ( 0                                                                              ),
  nReads_         ( 0                                                                              ),
  vc_             ( bldr->getSRPMuxVirtualChannel()                                                ),
  needsSwap_      ( (protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ? LE : BE) == hostByteOrder() ),
  needsPldSwap_   (  protoVersion_ == IProtoStackBuilder::SRP_UDP_V1                               ),
  tid_            ( 0                                                                              ),
  byteResolution_ (    bldr->getSRPVersion() >= IProtoStackBuilder::SRP_UDP_V3
                    && bldr->getSRPRetryCount() > 65535                                            ),
  maxWordsRx_     ( 0                                                                              ),
  maxWordsTx_     ( 0                                                                              ),
  defaultWriteMode_( bldr->getSRPDefaultWriteMode()
                   ),
  asyncXactMgr_   ( IAsyncIOTransactionManager::create( usrTimeout_.getUs() )                      ),
  asyncIOHandler_ ( asyncXactMgr_, this                                                            ),
  mutex_          ( CMtx::AttrRecursive(), "SRPADDR"                                               )
{
ProtoModSRPMux       srpMuxMod( dynamic_pointer_cast<ProtoModSRPMux::element_type>( stack->getProtoMod() ) );
int                  nbits;

	if ( hasRssi( stack ) ) {
		// have RSSI

		// FIXME: should find out dynamically what RSSI's retransmission
		//        timeout is and set the cap to a few times that value
		dynTimeout_.setTimeoutCap( 50000 ); // 50 ms for now
	}

	if ( ! srpMuxMod )
		throw InternalError("No SRP Mux? Found");


	tidLsb_ = 1 << srpMuxMod->getTidLsb();
	nbits   = srpMuxMod->getTidNumBits();
	tidMsk_ = (nbits > 31 ? 0xffffffff : ( (1<<nbits) - 1 ) ) << srpMuxMod->getTidLsb();

	asyncIOPort_ = srpMuxMod->createPort( vc_ | 0x80, bldr->getSRPMuxOutQueueDepth() );
}

void
CSRPAddressImpl::startProtoStack()
{
unsigned             maxwords;
ProtoPort            stack = getProtoStack();

	CCommAddressImpl::startProtoStack();

	// there seems to be a bug in either the depacketizer or SRP:
	// weird things happen if fragment payload is not 8-byte 
	// aligned
	mtu_        = (mtu_ / 8) * 8;

	maxwords    = mtu_ / sizeof(uint32_t);

	if ( protoVersion_ >= IProtoStackBuilder::SRP_UDP_V3 ) {
		maxwords -= 5;
	} else {
		maxwords -= 2;
		if ( protoVersion_ < IProtoStackBuilder::SRP_UDP_V2 ) {
			maxwords--;
		}
	}
	maxwords--; //tail/status

	maxWordsRx_ = (protoVersion_ < IProtoStackBuilder::SRP_UDP_V3 || !hasDepack( stack )) ? maxwords : (1<<28);
	maxWordsTx_ = maxWordsRx_;
	if ( maxWordsTx_ > MAXTXWORDS ) {
		maxWordsTx_ = MAXTXWORDS;
	}

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP: MTU is %d; maxwords %d, TX: %d, RX: %d\n", mtu_, maxwords, maxWordsRx_, maxWordsTx_);
#endif
}

void
CSRPAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	CCommAddressImpl::dumpYamlPart( node );
	YAML::Node srpParms;
	writeNode(srpParms, YAML_KEY_protocolVersion , protoVersion_      );
	writeNode(srpParms, YAML_KEY_timeoutUS       , usrTimeout_.getUs());
	writeNode(srpParms, YAML_KEY_dynTimeout      , useDynTimeout_     );
	writeNode(srpParms, YAML_KEY_retryCount      , retryCnt_          );
	writeNode(srpParms, YAML_KEY_defaultWriteMode, defaultWriteMode_  );
	writeNode(node, YAML_KEY_SRP, srpParms);
}

CSRPAddressImpl::~CSRPAddressImpl()
{
	shutdownProtoStack();
}

void CSRPAddressImpl::setTimeoutUs(unsigned timeoutUs)
{
	this->usrTimeout_.set(timeoutUs);
}

void CSRPAddressImpl::setRetryCount(unsigned retryCnt)
{
	this->retryCnt_ = retryCnt;
}

static void swp32(SRPWord *buf)
{
#ifdef __GNUC__
	*buf = __builtin_bswap32( *buf );
#else
	#error "bswap32 needs to be implemented"
#endif
}

static void swpw(uint8_t *buf)
{
uint32_t v;
	// gcc knows how to optimize this
	memcpy( &v, buf, sizeof(SRPWord) );
#ifdef __GNUC__
	v = __builtin_bswap32( v );
#else
	#error "bswap32 needs to be implemented"
#endif
	memcpy( buf, &v, sizeof(SRPWord) );
}

#ifdef SRPADDR_DEBUG
static void time_retry(struct timespec *then, int attempt, const char *pre, ProtoPort me)
{
struct timespec now;

	if ( clock_gettime(CLOCK_REALTIME, &now) ) {
		throw IOError("clock_gettime(time_retry) failed", errno);
	}
	if ( attempt > 0 ) {
		CTimeout xx(now);
		xx -= CTimeout(*then);
		printf(stderr, "%s -- retry (attempt %d) took %" PRId64 "us\n", pre, attempt, xx.getUs());
	}
	*then = now;
}
#endif

#define CMD_WRITE 0x40000000

#define CMD_READ_V3         0x000
#define CMD_WRITE_V3        0x100
#define CMD_POSTED_WRITE_V3 0x200

#define PROTO_VERS_3     3

static CFreeList<CSRPAsyncReadTransaction, CAsyncIOTransaction> srpReadTransactionPool;

uint32_t
CSRPAddressImpl::extractTid(BufChain rchn) const
{
uint32_t msgTid;

unsigned tidOff = getProtoVersion() == IProtoStackBuilder::SRP_UDP_V2 ? 0 : 4;

	if ( 0 == rchn->extract( &msgTid, tidOff, sizeof(msgTid) ) ) {
		throw InternalError("No TID found in SRP message\n");
	}
	if ( needsHdrSwap() ) {
		swp32( &msgTid );
	}
	return toTid( msgTid );
}

uint64_t CSRPAddressImpl::readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes, AsyncIO aio) const
{
SRPAsyncReadTransaction xact = srpReadTransactionPool.alloc();

	xact->reset( this, dst, off, sbytes );

	// post this transaction to the manager
	asyncXactMgr_->post( xact, xact->getTid(), aio );

	// If we are open then the async door must also be open
	xact->post( asyncIOHandler_.getDoor(), mtu_ );

	return 0;
}

uint64_t CSRPAddressImpl::readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const
{
#ifdef SRPADDR_DEBUG
struct timespec retry_then;
#endif

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP readBlk_unlocked off %" PRIx64 "; sbytes %d, protoVersion %d\n", off, sbytes, protoVersion_);
#endif

	if ( sbytes == 0 )
		return 0;

	CSRPReadTransaction xact( this, dst, off, sbytes );

	unsigned attempt = 0;

	do {
		uint32_t tidBits;
		BufChain rchn;

		xact.post( door_, mtu_ );

		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		do {

			rchn = door_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef SRPADDR_DEBUG
				time_retry( &retry_then, attempt, "READ", door_ );
#endif
				goto retry;
			}

			if ( clock_gettime(CLOCK_REALTIME, &now) ) {
				throw IOError("clock_gettime(now) failed", errno);
			}

			tidBits = extractTid( rchn );

		} while ( ! tidMatch( tidBits , xact.getTid() ) );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		xact.complete( rchn );

		return sbytes;

retry:
		if ( useDynTimeout_ )
			dynTimeout_.relax();
		nRetries_++;

	} while ( ++attempt <= retryCnt_ );

	if ( useDynTimeout_ )
		dynTimeout_.reset( usrTimeout_ );

	throw IOError("No response -- timeout");
}


int
CSRPAddressImpl::open (CompositePathIterator *node)
{
CMtx::lg guard( & doorMtx_ );

int rval = CAddressImpl::open( node );

	if ( 0 == rval ) {
		door_        = getProtoStack()->open();
		// open a second VC for asynchronous communication
		asyncIOHandler_.threadStart( asyncIOPort_->open() );
#ifdef SRPADDR_DEBUG
		fprintf(stderr, "SRP Open\n");
#endif
	}

	return rval;
}

int
CSRPAddressImpl::close(CompositePathIterator *node)
{
CMtx::lg guard( &doorMtx_ );

int rval = CAddressImpl::close( node );
	if ( 1 == rval ) {
#ifdef SRPADDR_DEBUG
		fprintf(stderr, "SRP Close\n");
#endif
		door_.reset();
		asyncIOHandler_.threadStop();
	}
	return rval;
}

uint64_t CSRPAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
uint64_t rval            = 0;
unsigned headbytes       = (byteResolution_ ? 0 : (args->off_ & (sizeof(SRPWord)-1)) );
uint64_t off             = args->off_;
uint8_t *dst             = args->dst_;
unsigned sbytes          = args->nbytes_;

unsigned totbytes;
unsigned nWords;

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	if ( args->aio_ ) {
		args->aio_ =  IAsyncIOParallelCompletion::create( args->aio_ );
	}

	{
	CMtx::lg GUARD( &mutex_ );

		while ( nWords > maxWordsRx_ ) {
			int nbytes = maxWordsRx_*4 - headbytes;
			if ( args->aio_ ) {
				rval   += readBlk_unlocked(node, args->cacheable_, dst, off, nbytes, args->aio_);
			} else {
				rval   += readBlk_unlocked(node, args->cacheable_, dst, off, nbytes);
			}
			nWords -= maxWordsRx_;
			sbytes -= nbytes;
			dst    += nbytes;
			off    += nbytes;
			headbytes = 0;
		}

		if ( args->aio_ ) {
			rval += readBlk_unlocked(node, args->cacheable_, dst, off, sbytes, args->aio_);
		} else {
			rval += readBlk_unlocked(node, args->cacheable_, dst, off, sbytes);
		}
	}

	nReads_++;

	return rval;
}

BufChain
CSRPAddressImpl::assembleXBuf(IOVec *iov, unsigned iovlen, int iov_pld, int toput) const
{
BufChain xchn   = IBufChain::create();

unsigned bufoff = 0, i;
int      j;

	for ( i=0; i<iovlen; i++ ) {
		xchn->insert( iov[i].iov_base, bufoff, iov[i].iov_len, mtu_ );
		bufoff += iov[i].iov_len;
	}

	if ( iov_pld >= 0 ) {
		bufoff = 0;
		for ( j=0; j<iov_pld; j++ )
			bufoff += iov[j].iov_len;
		Buf b = xchn->getHead();
		while ( b && bufoff >= b->getSize() ) {
			bufoff -= b->getSize();
			b = b->getNext();
		}
		j = 0;
		while ( b && j < toput ) {
			if ( bufoff + sizeof(SRPWord) <= b->getSize() ) {
				swpw( b->getPayload() + bufoff );
				bufoff += sizeof(SRPWord);
				j      += sizeof(SRPWord);
			} else {
				bufoff = 0;
				b = b->getNext();
			}
		}
	}

	return xchn;
}

uint64_t CSRPAddressImpl::writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
SRPWord  xbuf[5];
SRPWord  zero = 0;
SRPWord  pad  = 0;
uint8_t  first_word[sizeof(SRPWord)];
uint8_t  last_word[sizeof(SRPWord)];
int      j, put;
unsigned i;
unsigned headbytes       = ( byteResolution_ ? 0 : (off & (sizeof(SRPWord)-1)) );
unsigned totbytes;
IOVec    iov[5];
int      nWords;
uint32_t tid;
int      iov_pld = -1;
#ifdef SRPADDR_DEBUG
struct timespec retry_then = {0}; // initialize to avoid compiler warning
#endif
int      expected;
int      firstlen = 0, lastlen = 0; // silence compiler warning about un-initialized use
int      posted   = (    POSTED        == defaultWriteMode_
                      && protoVersion_ >= IProtoStackBuilder::SRP_UDP_V3 );

	if ( dbytes == 0 )
		return 0;

	// these look similar but are different...
	bool doSwapV1 = needsPayloadSwap();
	bool doSwap   = needsHdrSwap();

	totbytes = headbytes + dbytes;

	nWords = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP writeBlk_unlocked off %" PRIx64 "; dbytes %d, swapV1 %d, swap %d headbytes %i, totbytes %i, nWords %i, msk1 0x%02x, mskn 0x%02x\n", off, dbytes, doSwapV1, doSwap, headbytes, totbytes, nWords, msk1, mskn);
#endif


	bool merge_first = headbytes      || msk1;
	bool merge_last  = ( ! byteResolution_ && (totbytes & (sizeof(SRPWord)-1)) ) || mskn;

	if ( (merge_first || merge_last) && cacheable < IField::WB_CACHEABLE ) {
		throw IOError("Cannot merge bits/bytes to non-cacheable area");
	}

	int toput = dbytes;

	if ( merge_first ) {
		if ( merge_last && dbytes == 1 ) {
			// first and last byte overlap
			msk1 |= mskn;
			merge_last = false;
		}

		int first_byte = headbytes;
		int remaining;

		CReadArgs rargs;
		rargs.cacheable_  = cacheable;
		rargs.dst_        = first_word;

		bool lastbyte_in_firstword =  ( merge_last && totbytes <= (int)sizeof(SRPWord) );

		if ( byteResolution_ ) {
			if ( lastbyte_in_firstword ) {
				rargs.nbytes_ = totbytes; //lastbyte_in_firstword implies totbytes <= sizeof(SRPWord)
			} else {
				rargs.nbytes_ = 1;
			}
			rargs.off_    = off;
		} else {
			rargs.nbytes_ = sizeof(first_word);
			rargs.off_    = off & ~3ULL;
		}
		remaining = (totbytes <= rargs.nbytes_ ? totbytes : rargs.nbytes_) - first_byte - 1;

		firstlen  = rargs.nbytes_;

		read(node, &rargs);

		first_word[ first_byte  ] = (first_word[ first_byte ] & msk1) | (src[0] & ~msk1);

		toput--;

		if ( lastbyte_in_firstword ) {
			// totbytes must be <= sizeof(SRPWord) and > 0 (since last==first was handled above)
			int last_byte = (totbytes-1);
			first_word[ last_byte  ] = (first_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
			remaining--;
			toput--;
			merge_last = false;
		}

		if ( remaining > 0 ) {
			memcpy( first_word + first_byte + 1, src + 1, remaining );
			toput -= remaining;
		}
	}
	if ( merge_last ) {

		CReadArgs rargs;
		int       last_byte;

		rargs.cacheable_ = cacheable;
		rargs.dst_       = last_word;

		if ( byteResolution_ ) {
			rargs.nbytes_    = 1;
			rargs.off_       = off + dbytes - 1;
			last_byte        = 0;
		} else {
			rargs.nbytes_    = sizeof(last_word);
			rargs.off_       = (off + dbytes - 1) & ~3ULL;
			last_byte        = (totbytes-1) & (sizeof(SRPWord)-1);
		}

		lastlen = rargs.nbytes_;

		read(node, &rargs);

		last_word[ last_byte  ] = (last_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
		toput--;

		if ( last_byte > 0 ) {
			memcpy(last_word, &src[dbytes-1-last_byte], last_byte);
			toput -= last_byte;
		}
	}

	put = 0;
	tid = getTid();

	if ( protoVersion_ < IProtoStackBuilder::SRP_UDP_V3 ) {
		expected    = 3;
		if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
			xbuf[put++] = vc_ << 24;
			expected++;
		}
		xbuf[put++] = tid;
		xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;
	} else {
		xbuf[put++] = (posted ? CMD_POSTED_WRITE_V3 : CMD_WRITE_V3) | PROTO_VERS_3;
		xbuf[put++] = tid;
		xbuf[put++] = ( byteResolution_ ? off : (off & ~3ULL) );
		xbuf[put++] = off >> 32;
		xbuf[put++] = ( byteResolution_ ? totbytes : nWords << 2 ) - 1;
		expected    = 6;
	}

	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}

		swp32( &zero );
	}

	i = 0;
	iov[i].iov_base = xbuf;
	iov[i].iov_len  = sizeof(xbuf[0])*put;
	i++;

	if ( merge_first ) {
		iov[i].iov_len  = firstlen;
		if ( doSwapV1 )
			swpw( first_word );
		iov[i].iov_base = first_word;
		i++;
	}

	if ( toput > 0 ) {
		iov[i].iov_base = src + (merge_first ? (firstlen - headbytes) : 0);
		iov[i].iov_len  = toput;

		if ( doSwapV1 ) {
/* do NOT swap in the source buffer; defer until copied to
 * the transmit buffer
			for ( j = 0; j<toput; j += sizeof(SRPWord) ) {
				swpw( (uint8_t*)iov[i].iov_base + j );
			}
 */
			iov_pld = i;
		}
		i++;
	}

	if ( merge_last ) {
		if ( doSwapV1 )
			swpw( last_word );
		iov[i].iov_base = last_word;
		iov[i].iov_len  = lastlen;
		i++;
	}

	if ( protoVersion_ < IProtoStackBuilder::SRP_UDP_V3 ) {
		iov[i].iov_base = &zero;
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	} else if ( byteResolution_ && (totbytes & 3) ) {
		iov[i].iov_base = &pad;
		iov[i].iov_len  = sizeof(SRPWord) - (totbytes & 3);
		i++;
	}

	unsigned attempt = 0;
	unsigned iovlen  = i;

	CSRPWriteTransaction xact( this, nWords, expected );

	do {
		BufChain xchn = assembleXBuf(iov, iovlen, iov_pld, toput);

		BufChain rchn;
		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		door_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

		if ( posted )
			return dbytes;

		do {
			rchn = door_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef SRPADDR_DEBUG
				time_retry( &retry_then, attempt, "WRITE", door_ );
#endif
				goto retry;
			}

			if ( clock_gettime(CLOCK_REALTIME, &now) ) {
				throw IOError("clock_gettime(now) failed", errno);
			}

		} while ( ! tidMatch( extractTid( rchn ), tid ) );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		xact.complete( rchn );

		return dbytes;
retry:
		if ( useDynTimeout_ )
			dynTimeout_.relax();
		nRetries_++;
	} while ( ++attempt <= retryCnt_ );

	if ( useDynTimeout_ )
		dynTimeout_.reset( usrTimeout_ );

	throw IOError("Too many retries");
}

uint64_t CSRPAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
uint64_t rval            = 0;
unsigned headbytes       = (byteResolution_ ? 0 : (args->off_ & (sizeof(SRPWord)-1)) );
unsigned dbytes          = args->nbytes_;
uint64_t off             = args->off_;
uint8_t *src             = args->src_;
uint8_t  msk1            = args->msk1_;

unsigned totbytes;
unsigned nWords;

	if ( dbytes == 0 )
		return 0;

	totbytes = headbytes + dbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	CMtx::lg GUARD( &mutex_ );

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP writeBlk nWordsmaxWordsTx_ %d\n", maxWordsTx_);
#endif
	while ( nWords > maxWordsTx_ ) {
		int nbytes = maxWordsTx_*4 - headbytes;
		rval += writeBlk_unlocked(node, args->cacheable_, src, off, nbytes, msk1, 0);
		nWords -= maxWordsTx_;
		dbytes -= nbytes;
		src    += nbytes;
		off    += nbytes;
		headbytes = 0;
		msk1      = 0;
	}

	rval += writeBlk_unlocked(node, args->cacheable_, src, off, dbytes, msk1, args->mskn_);

	nWrites_++;
	return rval;

}

void CSRPAddressImpl::dump(FILE *f) const
{
	fprintf(f,"CSRPAddressImpl:\n");
	fprintf(f,"SRP Info:\n");
	fprintf(f,"  Protocol Version  : %8u\n",   protoVersion_);
	fprintf(f,"  Default Write Mode: %s\n",   SYNCHRONOUS == defaultWriteMode_ ? "Synchronous" : "Posted");
	fprintf(f,"  Timeout (user)    : %8" PRIu64 "us\n", usrTimeout_.getUs());
	fprintf(f,"  Timeout %s : %8" PRIu64 "us\n", useDynTimeout_ ? "(dynamic)" : "(capped) ", dynTimeout_.get().getUs());
	if ( useDynTimeout_ )
	{
	fprintf(f,"  max Roundtrip time: %8" PRIu64 "us\n", dynTimeout_.getMaxRndTrip().getUs());
	fprintf(f,"  avg Roundtrip time: %8" PRIu64 "us\n", dynTimeout_.getAvgRndTrip().getUs());
	}
	fprintf(f,"  Retry Limit       : %8u\n",   retryCnt_);
	fprintf(f,"  # of retried ops  : %8u\n",   nRetries_);
	fprintf(f,"  # of writes (OK)  : %8u\n",   nWrites_);
	fprintf(f,"  # of reads  (OK)  : %8u\n",   nReads_);
	fprintf(f,"  Virtual Channel   : %8u\n",   vc_);
	fprintf(f,"  Async Messages    : %8u\n",   asyncIOHandler_.getMsgCount());
	CCommAddressImpl::dump(f);
}

DynTimeout::DynTimeout(const CTimeout &iniv)
: timeoutCap_( CAP_US )
{
	reset( iniv );
}

void DynTimeout::setLastUpdate()
{
	if ( clock_gettime( CLOCK_MONOTONIC, &lastUpdate_.tv_ ) ) {
		throw IOError("clock_gettime failed! :", errno);
	}

	// when the timeout becomes too small then I experienced
	// some kind of scheduling problem where packets
	// arrive but not all threads processing them upstream
	// seem to get CPU time quickly enough.
	// We mitigate by capping the timeout if that happens.

	uint64_t new_timeout = avgRndTrip_ >> (AVG_SHFT-MARG_SHFT);
	if ( new_timeout < timeoutCap_ )
		new_timeout = timeoutCap_;
	dynTimeout_.set( new_timeout );
	nSinceLast_ = 0;
}

const CTimeout
DynTimeout::getAvgRndTrip() const
{
	return CTimeout( avgRndTrip_ >> AVG_SHFT );
}

void DynTimeout::reset(const CTimeout &iniv)
{
	maxRndTrip_.set(0);
	avgRndTrip_ = iniv.getUs() << (AVG_SHFT - MARG_SHFT);
	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	fprintf(stderr, "dynTimeout reset to %" PRId64 "\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::relax()
{

	// increase
	avgRndTrip_ += (dynTimeout_.getUs()<<1) - (avgRndTrip_ >> AVG_SHFT);

	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	fprintf(stderr, "RETRY (timeout %" PRId64 ")\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::update(const struct timespec *now, const struct timespec *then)
{
CTimeout diff(*now);
int64_t  diffus;

	diff -= CTimeout( *then );

	if ( maxRndTrip_ < diff )
		maxRndTrip_ = diff;

	avgRndTrip_ += (diffus = diff.getUs() - (avgRndTrip_ >> AVG_SHFT));

	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	fprintf(stderr, "dynTimeout update to %" PRId64 " (rnd %" PRId64 " -- diff %" PRId64 ", avg %" PRId64 " = 128*%" PRId64 ")\n",
		dynTimeout_.getUs(),
		diff.getUs(),
		diffus,
		avgRndTrip_,
		avgRndTrip_>>AVG_SHFT);
#endif
}
