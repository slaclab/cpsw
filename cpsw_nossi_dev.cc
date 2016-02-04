#include <cpsw_nossi_dev.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cpsw_proto_mod_depack.h>
#include <cpsw_proto_mod_udp.h>
#include <cpsw_proto_mod_srpmux.h>

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/locks.hpp>

using boost::recursive_mutex;
using boost::lock_guard;
using boost::dynamic_pointer_cast;

typedef shared_ptr<NoSsiDevImpl> NoSsiDevImplP;

typedef uint32_t SRPWord;

struct Mutex {
	recursive_mutex m;
};

//#define NOSSI_DEBUG
//#ifdef TIMEOUT_DEBUG

#define MAXWORDS 256

CUdpSRPAddressImpl::CUdpSRPAddressImpl(AKey k, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc)
:CCommAddressImpl(k),
 protoVersion_(version),
 dport_(dport),
 usrTimeout_(timeoutUs),
 dynTimeout_(usrTimeout_),
 retryCnt_(retryCnt),
 nRetries_(0),
 nWrites_(0),
 nReads_(0),
 vc_(vc),
 tid_(0),
 mutex_(0)
{
ProtoPortMatchParams cmp;
ProtoModSRPMux       srpMuxMod;
ProtoPort            srpPort;
NoSsiDevImpl         owner( getOwnerAs<NoSsiDevImpl>() );
int                  nbits;


	cmp.udpDestPort_  = dport;
	cmp.srpVersion_   = version;

	if ( (srpPort = owner->findProtoPort( &cmp )) ) {
		if ( ! (srpMuxMod = dynamic_pointer_cast<ProtoModSRPMux::element_type>( srpPort->getProtoMod() )) ) {
			throw InternalError("SRP port not attached to SRPMuxMod ?!");
		}
	} else {
		// create new
		struct sockaddr_in dst;

		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( dport );
		dst.sin_addr.s_addr = owner->getIpAddress();

		ProtoModUdp    udpMod = CShObj::create< ProtoModUdp >( &dst, 0/* no queue */, 1 /* nThreads */, 0 /* no poller */ );

		srpMuxMod = CShObj::create< ProtoModSRPMux >( version );

		udpMod->addAtPort( srpMuxMod );
	}

	protoStack_ = srpMuxMod->createPort( vc );

	tidLsb_ = 1 << srpMuxMod->getTidLsb();
	nbits   = srpMuxMod->getTidNumBits();
	tidMsk_ = (nbits > 31 ? 0xffffffff : ( (1<<nbits) - 1 ) ) << srpMuxMod->getTidLsb();

	mutex_ = new Mutex();
}

CUdpSRPAddressImpl::~CUdpSRPAddressImpl()
{
	if ( mutex_ )
		delete mutex_;
}

CNoSsiDevImpl::CNoSsiDevImpl(Key &k, const char *name, const char *ip)
: CDevImpl(k, name), ip_str_(ip ? ip : "ANY")
{
	if ( INADDR_NONE == ( d_ip_ = ip ? inet_addr( ip ) : INADDR_ANY ) ) {
		throw InvalidArgError( ip );
	}
}

void CUdpSRPAddressImpl::setTimeoutUs(unsigned timeoutUs)
{
	this->usrTimeout_.set(timeoutUs);
}

void CUdpSRPAddressImpl::setRetryCount(unsigned retryCnt)
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

#ifdef NOSSI_DEBUG
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

uint64_t CUdpSRPAddressImpl::readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const
{
SRPWord  bufh[4];
uint8_t  buft[sizeof(SRPWord)];
SRPWord  xbuf[5];
SRPWord  header;
SRPWord  status;
unsigned i;
int      j, put;
unsigned headbytes = (off & (sizeof(SRPWord)-1));
unsigned tailbytes = 0;
int      totbytes;
struct iovec  iov[5];
int      got;
int      nWords;
int      expected = 0;
uint32_t tid = getTid();
#ifdef NOSSI_DEBUG
struct timespec retry_then;
#endif


	if ( sbytes == 0 )
		return 0;

	// V1 sends payload in network byte order. We want to transform to restore
	// the standard AXI layout which is little-endian
	bool doSwapV1 = (INoSsiDev::SRP_UDP_V1 == protoVersion_);
	// header info needs to be swapped to host byte order since we interpret it
	bool doSwap   =	( ( protoVersion_ == INoSsiDev::SRP_UDP_V2 ? BE : LE ) == hostByteOrder() );

	totbytes = headbytes + sbytes;

	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	put = expected = 0;
	if ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ) {
		xbuf[put++] = vc_ << 24;
		expected++;
	}
	xbuf[put++] = tid;
	xbuf[put++] = (off >> 2) & 0x3fffffff;
	xbuf[put++] = nWords   - 1;
	xbuf[put++] = 0;
	expected += 3;

	// V2 uses LE, V1 network (aka BE) layout
	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}
	}

	i = 0;
	if ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ) {
		iov[i].iov_base = &header;
		iov[i].iov_len  = sizeof( header );
		i++;
	}
	iov[i].iov_base = bufh;
	iov[i].iov_len  = 8 + headbytes;
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( totbytes & (sizeof(SRPWord)-1) ) {
		tailbytes = sizeof(SRPWord) - (totbytes & (sizeof(SRPWord)-1));
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

		xchn->insert( xbuf, 0, sizeof(xbuf[0])*put );

		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		protoStack_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

		do {
			BufChain rchn = protoStack_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef NOSSI_DEBUG
				time_retry( &retry_then, attempt, "READ", protoStack_ );
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

#ifdef NOSSI_DEBUG
			printf("got %i bytes, off 0x%"PRIx64", sbytes %i, nWords %i\n", got, off, sbytes, nWords  );
			printf("got %i bytes\n", got);
			for (i=0; i<2; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);
			for ( i=0; i<headbytes; i++ ) {
				printf("headbyte[%i]: %x\n", i, ((uint8_t*)&bufh[2])[i]);
			}

			for ( i=0; (unsigned)i<sbytes; i++ ) {
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
			}

			for ( i=0; i < tailbytes; i++ ) {
				printf("tailbyte[%i]: %x\n", i, buft[i]);
			}
#endif
			if ( doSwap ) {
				swp32( &bufh[0] );
			}
		} while ( (bufh[0] & tidMsk_) != tid );

		dynTimeout_.update( &now, &then );

		if ( got != (int)sizeof(bufh[0])*(nWords + expected) ) {
			printf("got %i, nw %i, exp %i\n", got, nWords, expected);
			throw IOError("Received message truncated");
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
#ifdef NOSSI_DEBUG
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
#ifdef NOSSI_DEBUG
				for (i=0; i< sizeof(SRPWord); i++) printf("tailbytes tmp[%i]: %"PRIx8"\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst + hoff + j, tmp, sizeof(SRPWord) - tailbytes);
			}
#ifdef NOSSI_DEBUG
			for ( i=0; i< sbytes; i++ ) printf("swapped dst[%i]: %x\n", i, dst[i]);
#endif
		}
		if ( doSwap ) {
			swp32( &status );
			swp32( &header );
		}
		if ( status )
			throw BadStatusError("reading SRP", status);

		return sbytes;

retry:
		dynTimeout_.relax();
		nRetries_++;

	} while ( ++attempt <= retryCnt_ );

	dynTimeout_.reset( usrTimeout_ );

	throw IOError("No response -- timeout");
}
	
uint64_t CUdpSRPAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
uint64_t rval = 0;
unsigned headbytes = (args->off_ & (sizeof(SRPWord)-1));
int totbytes;
int nWords;
uint64_t off = args->off_;
uint8_t *dst = args->dst_;

unsigned sbytes = args->nbytes_;

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	lock_guard<recursive_mutex> GUARD( mutex_->m );

	while ( nWords > MAXWORDS ) {
		int nbytes = MAXWORDS*4 - headbytes;
		rval   += readBlk_unlocked(node, args->cacheable_, dst, off, nbytes);	
		nWords -= MAXWORDS;
		sbytes -= nbytes;	
		dst    += nbytes;
		off    += nbytes;
		headbytes = 0;
	}

	rval += readBlk_unlocked(node, args->cacheable_, dst, off, sbytes);

	nReads_++;

	return rval;
}


uint64_t CUdpSRPAddressImpl::writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
SRPWord  xbuf[4];
SRPWord  status;
SRPWord  header;
SRPWord  zero = 0;
uint8_t  first_word[sizeof(SRPWord)];
uint8_t  last_word[sizeof(SRPWord)];
int      j, put;
unsigned i;
int      headbytes = (off & (sizeof(SRPWord)-1));
int      totbytes;
struct iovec  iov[5];
int      got;
int      nWords;
uint32_t tid;
int      iov_pld = -1;
#ifdef NOSSI_DEBUG
struct timespec retry_then;
#endif

	if ( dbytes == 0 )
		return 0;

	// these look similar but are different...
	bool doSwapV1 =  (protoVersion_ == INoSsiDev::SRP_UDP_V1);
	bool doSwap   = ((protoVersion_ == INoSsiDev::SRP_UDP_V2 ? BE : LE) == hostByteOrder() );

	totbytes = headbytes + dbytes;

	nWords = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	bool merge_first = headbytes      || msk1;
	bool merge_last  = (totbytes & (sizeof(SRPWord)-1)) || mskn;

	int toput = dbytes;

	if ( merge_first ) {
		if ( merge_last && dbytes == 1 ) {
			// first and last byte overlap
			msk1 |= mskn;
			merge_last = false;
		}

		int first_byte = headbytes;

		CReadArgs rargs;
		rargs.cacheable_ = cacheable;
		rargs.dst_       = first_word;
		rargs.nbytes_    = sizeof(first_word);
		rargs.off_       = off & ~3ULL;

		read(node, &rargs);

		first_word[ first_byte  ] = (first_word[ first_byte ] & msk1) | (src[0] & ~msk1);

		toput--;

		int remaining = (totbytes <= (int)sizeof(SRPWord) ? totbytes : sizeof(SRPWord)) - first_byte - 1;

		if ( merge_last && totbytes <= (int)sizeof(SRPWord) ) {
			// last byte is also in first word
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
		rargs.cacheable_ = cacheable;
		rargs.dst_       = last_word;
		rargs.nbytes_    = sizeof(last_word);
		rargs.off_       = (off + dbytes) & ~3ULL;

		read(node, &rargs);

		int last_byte = (totbytes-1) & (sizeof(SRPWord)-1);
		last_word[ last_byte  ] = (last_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
		toput--;

		if ( last_byte > 0 ) {
			memcpy(last_word, &src[dbytes-1-last_byte], last_byte);
			toput -= last_byte;
		}
	}

	put = 0;
	if ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ) {
		xbuf[put++] = vc_ << 24;
	}
	tid         = getTid();
	xbuf[put++] = tid;
	xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;

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
		if ( doSwapV1 )
			swpw( first_word );
		iov[i].iov_base = first_word;
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	}

	if ( toput > 0 ) {
		iov[i].iov_base = src + (merge_first ? sizeof(SRPWord) - headbytes : 0);
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
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	}

	iov[i].iov_base = &zero;
	iov[i].iov_len  = sizeof(SRPWord);
	i++;

	// keep buffer chain around for retransmissions...
	BufChain xchn   = IBufChain::create();

	unsigned bufoff = 0;
	unsigned iovlen  = i;
	unsigned attempt = 0;
	uint32_t got_tid;

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

	do {
		int     bufsz = (nWords + 3)*sizeof(SRPWord);
		if ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ) {
			bufsz += sizeof(SRPWord);
		}
		BufChain rchn;
		uint8_t *rbuf;
		struct timespec then, now;
		if ( clock_gettime(CLOCK_REALTIME, &then) ) {
			throw IOError("clock_gettime(then) failed", errno);
		}

		protoStack_->push( xchn, 0, IProtoPort::REL_TIMEOUT );

		do {
			rchn = protoStack_->pop( dynTimeout_.getp(), IProtoPort::REL_TIMEOUT );
			if ( ! rchn ) {
#ifdef NOSSI_DEBUG
				time_retry( &retry_then, attempt, "WRITE", protoStack_ );
#endif
				goto retry;
			}

			if ( clock_gettime(CLOCK_REALTIME, &now) ) {
				throw IOError("clock_gettime(now) failed", errno);
			}

			got = rchn->getSize();
			if ( rchn->getLen() > 1 )
				throw InternalError("Assume received payload fits into a single buffer");
			rbuf = rchn->getHead()->getPayload();
#if 0
			printf("got %i bytes, dbytes %i, nWords %i\n", got, dbytes, nWords);
			printf("got %i bytes\n", got);
			for (i=0; i<2; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);

			for ( i=0; i<dbytes; i++ )
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			memcpy( &got_tid, rbuf + ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ? sizeof(SRPWord) : 0 ), sizeof(SRPWord) );
			if ( doSwap ) {
				swp32( &got_tid );
			}
		} while ( tid != (got_tid & tidMsk_) );

		dynTimeout_.update( &now, &then );

		if ( got != bufsz ) {
			throw IOError("read return value didn't match buffer size");
		}
		if ( protoVersion_ == INoSsiDev::SRP_UDP_V1 ) {
			if ( LE == hostByteOrder() ) {
				swpw( rbuf );
			}
			memcpy( &header, rbuf,  sizeof(SRPWord) );
		} else {
			header = 0;
		}
		if ( doSwap ) {
			swpw( rbuf + got - sizeof(SRPWord) );
		}
		memcpy( &status, rbuf + got - sizeof(SRPWord), sizeof(SRPWord) );
		if ( status )
			throw BadStatusError("writing SRP", status);
		return dbytes;
retry:
		dynTimeout_.relax();
		nRetries_++;
	} while ( ++attempt <= retryCnt_ );

	dynTimeout_.reset( usrTimeout_ );

	throw IOError("Too many retries");
}

uint64_t CUdpSRPAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
uint64_t rval = 0;
int headbytes = (args->off_ & (sizeof(SRPWord)-1));
int totbytes;
int nWords;
unsigned dbytes = args->nbytes_;
uint64_t off    = args->off_;
uint8_t *src    = args->src_;
uint8_t  msk1   = args->msk1_;

	if ( dbytes == 0 )
		return 0;

	totbytes = headbytes + dbytes;
	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	lock_guard<recursive_mutex> GUARD( mutex_->m );

	while ( nWords > MAXWORDS ) {
		int nbytes = MAXWORDS*4 - headbytes;
		rval += writeBlk_unlocked(node, args->cacheable_, src, off, nbytes, msk1, 0);	
		nWords -= MAXWORDS;
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

void CUdpSRPAddressImpl::dump(FILE *f) const
{
	fprintf(f,"CUdpSRPAddressImpl:\n");
	CAddressImpl::dump(f);
	fprintf(f,"\nPeer: %s:%d\n", getOwnerAs<NoSsiDevImpl>()->getIpAddressString(), dport_);
	fprintf(f,"  SRP Protocol Version: %8u\n",   protoVersion_);
	fprintf(f,"  Timeout (user)      : %8"PRIu64"us\n", usrTimeout_.getUs());
	fprintf(f,"  Timeout (dynamic)   : %8"PRIu64"us\n", dynTimeout_.get().getUs());
	fprintf(f,"  max Roundtrip time  : %8"PRIu64"us\n", dynTimeout_.getMaxRndTrip().getUs());
	fprintf(f,"  avg Roundtrip time  : %8"PRIu64"us\n", dynTimeout_.getAvgRndTrip().getUs());
	fprintf(f,"  Retry Limit         : %8u\n",   retryCnt_);
	fprintf(f,"  # of retried ops    : %8u\n",   nRetries_);
	fprintf(f,"  # of writes (OK)    : %8u\n",   nWrites_);
	fprintf(f,"  # of reads  (OK)    : %8u\n",   nReads_);
	fprintf(f,"  Virtual Channel     : %8u\n",   vc_);
}

bool CNoSsiDevImpl::portInUse(unsigned port)
{
ProtoPortMatchParams cmp;

	cmp.udpDestPort_=port;

	return findProtoPort( &cmp ) != 0;
}

void CNoSsiDevImpl::addAtAddress(Field child, INoSsiDev::ProtocolVersion version, unsigned dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc)
{
	IAddress::AKey k = getAKey();
	add( make_shared<CUdpSRPAddressImpl>(k, version, dport, timeoutUs, retryCnt, vc), child );
}

void CNoSsiDevImpl::addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads)
{
	if ( portInUse( dport ) ) {
		throw InvalidArgError("Cannot address same destination port from multiple instances");
	}
	IAddress::AKey k = getAKey();
	add( make_shared<CUdpStreamAddressImpl>(k, dport, timeoutUs, inQDepth, outQDepth, ldFrameWinSize, ldFragWinSize, nUdpThreads), child );
}


NoSsiDev INoSsiDev::create(const char *name, const char *ipaddr)
{
	return CShObj::create<NoSsiDevImpl>(name, ipaddr);
}

void CNoSsiDevImpl::setLocked()
{
	if ( getLocked() ) {
		throw InternalError("Cannot attach this type of device multiple times -- need to create a new instance");
	}
	CDevImpl::setLocked();
}

ProtoPort CNoSsiDevImpl::findProtoPort(ProtoPortMatchParams *cmp_p)
{
Children myChildren = getChildren();

Children::element_type::iterator it;

	for ( it = myChildren->begin(); it != myChildren->end(); ++it ) {
		shared_ptr<const CCommAddressImpl> child = static_pointer_cast<const CCommAddressImpl>( *it );
		// 'match' modifies the parameters (storing results); use a copy
		ProtoPortMatchParams cmp = *cmp_p;
		if ( cmp.requestedMatches() == child->getProtoStack()->match( &cmp ) )
			return child->getProtoStack();
	}
	return ProtoPort();
}

CUdpStreamAddressImpl::CUdpStreamAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads)
:CCommAddressImpl(key),
 dport_(dport)
{
struct sockaddr_in dst;
NoSsiDevImpl owner( getOwnerAs<NoSsiDevImpl>() );

	dst.sin_family      = AF_INET;
	dst.sin_port        = htons( dport );
	dst.sin_addr.s_addr = owner->getIpAddress();

	ProtoModUdp    udpMod     = CShObj::create< ProtoModUdp >( &dst, inQDepth, nUdpThreads );

	ProtoModDepack depackMod  = CShObj::create< ProtoModDepack >( outQDepth, ldFrameWinSize, ldFragWinSize, timeoutUs );

	udpMod->addAtPort( depackMod );

	protoStack_ = depackMod;
}

void CUdpStreamAddressImpl::dump(FILE *f) const
{
	fprintf(f,"CUdpStreamAddressImpl:\n");
	CAddressImpl::dump(f);
	fprintf(f,"\nPeer: %s:%d\n", getOwnerAs<NoSsiDevImpl>()->getIpAddressString(), dport_);
	if ( protoStack_ ) {
		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			m->dumpInfo( f );
		}
	}
}

uint64_t CUdpStreamAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
BufChain bch;

	bch = protoStack_->pop( &args->timeout_, IProtoPort::REL_TIMEOUT  );

	if ( ! bch )
		return 0;

	return bch->extract( args->dst_, args->off_, args->nbytes_ );
}

uint64_t CUdpStreamAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
BufChain bch = IBufChain::create();
uint64_t rval;

// Reserve some headroom in case protocols want to use it (they could prepend buffers
// but reserving is more efficient).
static const unsigned HEADROOM=32;

	bch->insert( args->src_, args->off_ + HEADROOM, args->nbytes_ );

	Buf b( bch->getHead() );
	if ( ! b ) {
		throw InternalError("No buffer after insert?");
	}
	b->setPayload( b->getPayload() + HEADROOM );

	rval = bch->getSize();

	protoStack_->push( bch, &args->timeout_, IProtoPort::REL_TIMEOUT );

	return rval;
}

DynTimeout::DynTimeout(const CTimeout &iniv)
{
	reset( iniv );
}

void DynTimeout::setLastUpdate()
{
	if ( clock_gettime( CLOCK_MONOTONIC, &lastUpdate_.tv_ ) ) {
		throw IOError("clock_gettime failed! :", errno);
	}
	uint64_t new_timeout = avgRndTrip_ >> (AVG_SHFT-MARG_SHFT);
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
	avgRndTrip_ = iniv.getUs() << AVG_SHFT;
	setLastUpdate();
#ifdef TIMEOUT_DEBUG
	printf("dynTimeout reset to %"PRId64"\n", dynTimeout_.getUs());
#endif
}

void DynTimeout::relax()
{
uint64_t timeout_cap = CAP_US<<(AVG_SHFT-MARG_SHFT);
	// when the timeout becomes too small then I experienced
	// some kind of scheduling problem where packets seem to
	// arrive but not all threads processing them upstream
	// seem to get CPU time quickly enough.
	// We mitigate by capping the timeout if that happens.
	if ( avgRndTrip_ < timeout_cap ) {
		avgRndTrip_ = timeout_cap;
		setLastUpdate();
	}
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
