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

#include <cpsw_proto_mod_srpmux.h>

#include <cpsw_mutex.h>

#include <cpsw_yaml.h>

using boost::dynamic_pointer_cast;

typedef uint32_t SRPWord;

//#define SRPADDR_DEBUG
//#define TIMEOUT_DEBUG

// if RSSI is used then AFAIK the max. segment size
// (including RSSI header) is 1024 octets.
// Must subtract RSSI (8), packetizer (9), SRP (V3: 20 + 4)
#define MAXWORDS (256 - 11 - 4)

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


CSRPAddressImpl::CSRPAddressImpl(AKey key, ProtoStackBuilder bldr, ProtoPort stack)
:CCommAddressImpl(key, stack),
 protoVersion_(bldr->getSRPVersion()),
 usrTimeout_(bldr->getSRPTimeoutUS()),
 dynTimeout_(usrTimeout_),
 useDynTimeout_(bldr->hasSRPDynTimeout()),
 retryCnt_(bldr->getSRPRetryCount() & 0xffff), // undocumented hack to test byte-resolution access
 nRetries_(0),
 nWrites_(0),
 nReads_(0),
 vc_(bldr->getSRPMuxVirtualChannel()),
 tid_(0),
 byteResolution_( bldr->getSRPVersion() >= IProtoStackBuilder::SRP_UDP_V3 && bldr->getSRPRetryCount() > 65535 ),
 maxWordsRx_( protoVersion_ < IProtoStackBuilder::SRP_UDP_V3 || !hasDepack( stack) ? MAXWORDS : (1<<28) ),
 maxWordsTx_( MAXWORDS ),
 mutex_( CMtx::AttrRecursive(), "SRPADDR" )
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
}

void
CSRPAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	CCommAddressImpl::dumpYamlPart( node );
	YAML::Node srpParms;
	writeNode(srpParms, YAML_KEY_protocolVersion, protoVersion_      );
	writeNode(srpParms, YAML_KEY_timeoutUS      , usrTimeout_.getUs());
	writeNode(srpParms, YAML_KEY_dynTimeout     , useDynTimeout_     );
	writeNode(srpParms, YAML_KEY_retryCount     , retryCnt_          );
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
		printf("%s -- retry (attempt %d) took %"PRId64"us\n", pre, attempt, xx.getUs());
	}
	*then = now;
}
#endif

#define CMD_READ  0x00000000
#define CMD_WRITE 0x40000000

#define CMD_READ_V3  0x000
#define CMD_WRITE_V3 0x100

#define PROTO_VERS_3     3

typedef struct srp_iovec {
	void  *iov_base;
	size_t iov_len;
} IOVec;

uint64_t CSRPAddressImpl::readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const
{
SRPWord  bufh[5];
unsigned hwrds;
uint8_t  buft[sizeof(SRPWord)];
SRPWord  xbuf[5];
SRPWord  header;
SRPWord  status;
unsigned i;
int      j, put;
unsigned headbytes = (byteResolution_ ? 0 : (off & (sizeof(SRPWord)-1)) );
unsigned tailbytes = 0;
int      totbytes;
IOVec    iov[5];
int      got;
int      nWords;
int      expected = 0;
int      tidoffwrd;
uint32_t tid = getTid();
#ifdef SRPADDR_DEBUG
struct timespec retry_then;
#endif

	// V1 sends payload in network byte order. We want to transform to restore
	// the standard AXI layout which is little-endian
	bool doSwapV1 = (IProtoStackBuilder::SRP_UDP_V1 == protoVersion_);
	// header info needs to be swapped to host byte order since we interpret it
	bool doSwap   =	( ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ? LE : BE ) == hostByteOrder() );

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP readBlk_unlocked off %"PRIx64"; sbytes %d, swapV1 %d, swap %d protoVersion %d\n", off, sbytes, doSwapV1, doSwap, protoVersion_);
#endif

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;

	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	put = expected = 0;
	if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
		xbuf[put++] = vc_ << 24;
		expected += 1;
	}
	if ( protoVersion_ < IProtoStackBuilder::SRP_UDP_V3 ) {
		xbuf[put++] = tid;
		xbuf[put++] = (off >> 2) & 0x3fffffff;
		xbuf[put++] = nWords   - 1;
		xbuf[put++] = 0;
		expected += 3;
		hwrds       = 2;
		tidoffwrd   = 0;
	} else {
		xbuf[put++] = CMD_READ_V3 | PROTO_VERS_3;
		xbuf[put++] = tid;
		xbuf[put++] =   byteResolution_ ? off       : off & ~3ULL;
		xbuf[put++] = off >> 32;
		xbuf[put++] = ( byteResolution_ ? totbytes : (nWords << 2) ) - 1;
		expected += 6;
		hwrds       = 5;
		tidoffwrd   = 1;
	}

	// V2,V3 uses LE, V1 network (aka BE) layout
	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}
	}

	i = 0;
	if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
		iov[i].iov_base = &header;
		iov[i].iov_len  = sizeof( header );
		i++;
	}
	iov[i].iov_base = bufh;
	iov[i].iov_len  = hwrds*sizeof(SRPWord) + headbytes;
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( totbytes & (sizeof(SRPWord)-1) ) {
		tailbytes       = sizeof(SRPWord) - (totbytes & (sizeof(SRPWord)-1));
		// padding if not word-aligned
		iov[i].iov_base = buft;
		iov[i].iov_len  = tailbytes;
		i++;
	}

	iov[i].iov_base = &status;
	iov[i].iov_len  = sizeof(SRPWord);
	i++;

	unsigned iovlen  = i;
	unsigned attempt = 0;

	do {
		BufChain xchn = IBufChain::create();
		BufChain rchn;

		xchn->insert( xbuf, 0, sizeof(xbuf[0])*put );

		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		door_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

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

			got = rchn->getSize();

			unsigned bufoff = 0;

			for ( i=0; i<iovlen; i++ ) {
				rchn->extract(iov[i].iov_base, bufoff, iov[i].iov_len);
				bufoff += iov[i].iov_len;
			}

#ifdef SRPADDR_DEBUG
			printf("got %i bytes, off 0x%"PRIx64", sbytes %i, nWords %i\n", got, off, sbytes, nWords  );
			printf("got %i bytes\n", got);
			for (i=0; i<hwrds; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);
			for ( i=0; i<headbytes; i++ ) {
				printf("headbyte[%i]: %x\n", i, ((uint8_t*)&bufh[hwrds])[i]);
			}

			for ( i=0; (unsigned)i<sbytes; i++ ) {
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
			}

			for ( i=0; i < tailbytes; i++ ) {
				printf("tailbyte[%i]: %x\n", i, buft[i]);
			}
#endif
			if ( doSwap ) {
				swp32( &bufh[tidoffwrd] );
			}
		} while ( (bufh[tidoffwrd] & tidMsk_) != tid );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		if ( got != (int)sizeof(bufh[0])*(nWords + expected) ) {
			if ( got < (int)sizeof(bufh[0])*expected ) {
				printf("got %i, nw %i, exp %i\n", got, nWords, expected);
				throw IOError("Received message (read response) truncated");
			} else {
				rchn->extract( &status, got - sizeof(status), sizeof(status) );
				if ( doSwap )
					swp32( &status );
				throw BadStatusError("SRP Read terminated with bad status", status);
			}
		}

		if ( doSwapV1 ) {
			// switch payload back to LE
			uint8_t  tmp[sizeof(SRPWord)];
			unsigned hoff = 0;
			if ( headbytes ) {
				hoff += sizeof(SRPWord) - headbytes;
				memcpy(tmp, &bufh[2], headbytes);
				if ( hoff > sbytes ) {
					// special case where a single word covers bufh, dst and buft
					memcpy(tmp+headbytes,        dst , sbytes);
					// note: since headbytes < 4 -> hoff < 4
					//       and since sbytes < hoff -> sbytes < 4 -> sbytes == sbytes % 4
					//       then we have: headbytes = off % 4
					//                     totbytes  = off % 4 + sbytes  and totbytes % 4 == (off + sbytes) % 4 == headbytes + sbytes
					//                     tailbytes =  4 - (totbytes % 4)
					//       and thus: headbytes + tailbytes + sbytes  == 4
					memcpy(tmp+headbytes+sbytes, buft, tailbytes);
				} else {
					memcpy(tmp+headbytes, dst, hoff);
				}
#ifdef SRPADDR_DEBUG
				for (i=0; i< sizeof(SRPWord); i++) printf("headbytes tmp[%i]: %x\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst, tmp+headbytes, hoff);
			}
			int jend =(((int)(sbytes - hoff)) & ~(sizeof(SRPWord)-1));
			for ( j = 0; j < jend; j+=sizeof(SRPWord) ) {
				swpw( dst + hoff + j );
			}
			if ( tailbytes && (hoff <= sbytes) ) { // cover the special case mentioned above
				memcpy(tmp, dst + hoff + j, sizeof(SRPWord) - tailbytes);
				memcpy(tmp + sizeof(SRPWord) - tailbytes, buft, tailbytes);
#ifdef SRPADDR_DEBUG
				for (i=0; i< sizeof(SRPWord); i++) printf("tailbytes tmp[%i]: %"PRIx8"\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst + hoff + j, tmp, sizeof(SRPWord) - tailbytes);
			}
#ifdef SRPADDR_DEBUG
			for ( i=0; i< sbytes; i++ ) printf("swapped dst[%i]: %x\n", i, dst[i]);
#endif
		}
		if ( doSwap ) {
			swp32( &status );
			swp32( &header );
		}
		if ( status ) {
			throw BadStatusError("reading SRP", status);
		}

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

	CMtx::lg GUARD( &mutex_ );

	while ( nWords > maxWordsRx_ ) {
		int nbytes = maxWordsRx_*4 - headbytes;
		rval   += readBlk_unlocked(node, args->cacheable_, dst, off, nbytes);	
		nWords -= maxWordsRx_;
		sbytes -= nbytes;	
		dst    += nbytes;
		off    += nbytes;
		headbytes = 0;
	}

	rval += readBlk_unlocked(node, args->cacheable_, dst, off, sbytes);

	nReads_++;

	return rval;
}

static BufChain assembleXBuf(IOVec *iov, unsigned iovlen, int iov_pld, int toput)
{
BufChain xchn   = IBufChain::create();

unsigned bufoff = 0, i;
int      j;

	for ( i=0; i<iovlen; i++ ) {
		xchn->insert(iov[i].iov_base, bufoff, iov[i].iov_len);
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
SRPWord  status;
SRPWord  header;
SRPWord  zero = 0;
SRPWord  pad  = 0;
uint8_t  first_word[sizeof(SRPWord)];
uint8_t  last_word[sizeof(SRPWord)];
int      j, put;
unsigned i;
unsigned headbytes       = ( byteResolution_ ? 0 : (off & (sizeof(SRPWord)-1)) );
unsigned totbytes;
IOVec    iov[5];
int      got;
int      nWords;
uint32_t tid;
int      iov_pld = -1;
#ifdef SRPADDR_DEBUG
struct timespec retry_then;
#endif
int      expected;
int      tidoff;
int      firstlen = 0, lastlen = 0; // silence compiler warning about un-initialized use

	if ( dbytes == 0 )
		return 0;

	// these look similar but are different...
	bool doSwapV1 =  (protoVersion_ == IProtoStackBuilder::SRP_UDP_V1);
	bool doSwap   = ((protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ? LE : BE) == hostByteOrder() );

	totbytes = headbytes + dbytes;

	nWords = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

#ifdef SRPADDR_DEBUG
	fprintf(stderr, "SRP writeBlk_unlocked off %"PRIx64"; dbytes %d, swapV1 %d, swap %d headbytes %i, totbytes %i, nWords %i, msk1 0x%02x, mskn 0x%02x\n", off, dbytes, doSwapV1, doSwap, headbytes, totbytes, nWords, msk1, mskn);
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
		tidoff      = 0;
		if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
			xbuf[put++] = vc_ << 24;
			expected++;
			tidoff  = 4;
		}
		xbuf[put++] = tid;
		xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;
	} else {
		xbuf[put++] = CMD_WRITE_V3 | PROTO_VERS_3;
		xbuf[put++] = tid;
		xbuf[put++] = ( byteResolution_ ? off : (off & ~3ULL) );
		xbuf[put++] = off >> 32;
		xbuf[put++] = ( byteResolution_ ? totbytes : nWords << 2 ) - 1;
		expected    = 6;
		tidoff      = 4;
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
	uint32_t got_tid;
	unsigned iovlen  = i;

	do {

		BufChain xchn = assembleXBuf(iov, iovlen, iov_pld, toput);

		BufChain rchn;
		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		door_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

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

			got = rchn->getSize();

			rchn->extract( &got_tid, tidoff, sizeof(SRPWord) );
			if ( doSwap ) {
				swp32( &got_tid );
			}

		} while ( tid != (got_tid & tidMsk_) );

		if ( useDynTimeout_ )
			dynTimeout_.update( &now, &then );

		if ( got != (int)sizeof(SRPWord)*(nWords + expected) ) {
			if ( got < (int)sizeof(SRPWord)*expected ) {
				printf("got %i, nw %i, exp %i\n", got, nWords, expected);
				throw IOError("Received message (write response) truncated");
			} else {
				rchn->extract( &status, got - sizeof(status), sizeof(status) );
				if ( doSwap )
					swp32( &status );
				throw BadStatusError("SRP Write terminated with bad status", status);
			}
		}

		if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
			rchn->extract( &header, 0, sizeof(header) );
			if ( LE == hostByteOrder() ) {
				swp32( &header );
			}
		} else {
			header = 0;
		}
		rchn->extract( &status, got - sizeof(SRPWord), sizeof(SRPWord) );
		if ( doSwap ) {
			swp32( &status );
		}
		if ( status )
			throw BadStatusError("writing SRP", status);
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
	fprintf(f,"  Timeout (user)    : %8"PRIu64"us\n", usrTimeout_.getUs());
	fprintf(f,"  Timeout %s : %8"PRIu64"us\n", useDynTimeout_ ? "(dynamic)" : "(capped) ", dynTimeout_.get().getUs());
	if ( useDynTimeout_ )
	{
	fprintf(f,"  max Roundtrip time: %8"PRIu64"us\n", dynTimeout_.getMaxRndTrip().getUs());
	fprintf(f,"  avg Roundtrip time: %8"PRIu64"us\n", dynTimeout_.getAvgRndTrip().getUs());
	}
	fprintf(f,"  Retry Limit       : %8u\n",   retryCnt_);
	fprintf(f,"  # of retried ops  : %8u\n",   nRetries_);
	fprintf(f,"  # of writes (OK)  : %8u\n",   nWrites_);
	fprintf(f,"  # of reads  (OK)  : %8u\n",   nReads_);
	fprintf(f,"  Virtual Channel   : %8u\n",   vc_);
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
	printf("dynTimeout reset to %"PRId64"\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::relax()
{

	// increase
	avgRndTrip_ += (dynTimeout_.getUs()<<1) - (avgRndTrip_ >> AVG_SHFT);

	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	printf("RETRY (timeout %"PRId64")\n", dynTimeout_.getUs());
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
	printf("dynTimeout update to %"PRId64" (rnd %"PRId64" -- diff %"PRId64", avg %"PRId64" = 128*%"PRId64")\n",
		dynTimeout_.getUs(),
		diff.getUs(),
		diffus,
		avgRndTrip_,
		avgRndTrip_>>AVG_SHFT);
#endif
}