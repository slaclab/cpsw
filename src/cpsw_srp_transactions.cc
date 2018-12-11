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

#define CMD_READ  0x00000000
#define CMD_WRITE 0x40000000

#define CMD_READ_V3  0x000
#define CMD_WRITE_V3 0x100

#define PROTO_VERS_3     3

//#define SRPADDR_DEBUG 0

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

CSRPAsyncReadTransaction::CSRPAsyncReadTransaction(
		const CAsyncIOTransactionKey &key)
: CAsyncIOTransaction( key ),
  CSRPReadTransaction( 0, 0, 0, 0 )
{
}

CSRPTransaction::CSRPTransaction(
	const CSRPAddressImpl *srpAddr
)
{
	if ( srpAddr )
		reset(srpAddr);
}

void
CSRPTransaction::reset(
	const CSRPAddressImpl *srpAddr
)
{
	tid_      = srpAddr->getTid();
	doSwapV1_ = srpAddr->needsPayloadSwap();
	doSwap_   = srpAddr->needsHdrSwap();
}

CSRPWriteTransaction::CSRPWriteTransaction(
	const CSRPAddressImpl *srpAddr,
	int                    nWords,
	int                    expected
) : CSRPTransaction( srpAddr )
{
	if ( srpAddr )
		resetMe( srpAddr, nWords, expected );
}

void
CSRPWriteTransaction::resetMe(
	const CSRPAddressImpl *srpAddr,
	int                    nWords,
	int                    expected
)
{
	nWords_   = nWords;
	expected_ = expected;
}

void
CSRPWriteTransaction::reset(
	const CSRPAddressImpl *srpAddr,
	int                    nWords,
	int                    expected
)
{
	CSRPTransaction::reset( srpAddr );
	resetMe( srpAddr, nWords, expected );
}

CSRPReadTransaction::CSRPReadTransaction(
	const CSRPAddressImpl *srpAddr,
	uint8_t               *dst,
	uint64_t               off,
	unsigned               sbytes
) : CSRPTransaction( srpAddr )
{
	if ( srpAddr )
		resetMe( srpAddr, dst, off, sbytes );
}

void
CSRPReadTransaction::reset(
	const CSRPAddressImpl *srpAddr,
	uint8_t               *dst,
	uint64_t               off,
	unsigned               sbytes
)
{
	CSRPTransaction::reset( srpAddr );
	resetMe( srpAddr, dst, off, sbytes );
}

void
CSRPReadTransaction::resetMe(
	const CSRPAddressImpl *srpAddr,
	uint8_t               *dst,
	uint64_t               off,
	unsigned               sbytes
)
{
int	     nWords;

	dst_      = dst;
	off_      = off;
	sbytes_   = sbytes;
	headBytes_= srpAddr->getByteResolution() ? 0 : (off_ & (sizeof(SRPWord)-1));
	xbufWords_= 0;
	expected_ = 0;
	iovLen_   = 0;
	tailBytes_= 0;

	nWords = getNWords();

	if ( srpAddr->getProtoVersion() == IProtoStackBuilder::SRP_UDP_V1 ) {
		xbuf_[xbufWords_++] = srpAddr->getVC() << 24;
		expected_ += 1;
	}
	if ( srpAddr->getProtoVersion() < IProtoStackBuilder::SRP_UDP_V3 ) {
		xbuf_[xbufWords_++] = getTid();
		xbuf_[xbufWords_++] = ( (off_ >> 2) & 0x3fffffff ) | CMD_READ;
		xbuf_[xbufWords_++] = nWords   - 1;
		xbuf_[xbufWords_++] = 0;
		expected_          += 3;
		hdrWords_           = 2;
	} else {
		xbuf_[xbufWords_++] = CMD_READ_V3 | PROTO_VERS_3;
		xbuf_[xbufWords_++] = getTid();
		xbuf_[xbufWords_++] =   srpAddr->getByteResolution() ? off_          : (off_ & ~3ULL);
		xbuf_[xbufWords_++] = off_ >> 32;
		xbuf_[xbufWords_++] = ( srpAddr->getByteResolution() ? getTotbytes() : (nWords << 2) ) - 1;
		expected_          += 6;
		hdrWords_           = 5;
	}

	// V2,V3 uses LE, V1 network (aka BE) layout
	if ( needsHdrSwap() ) {
		for ( int j=0; j<xbufWords_; j++ ) {
			swp32( &xbuf_[j] );
		}
	}

	if ( srpAddr->getProtoVersion() == IProtoStackBuilder::SRP_UDP_V1 ) {
		iov_[iovLen_].iov_base = &v1Header_;
		iov_[iovLen_].iov_len  = sizeof( v1Header_ );
		iovLen_++;
	}

	if ( getTotbytes() & (sizeof(SRPWord)-1) ) {
		tailBytes_      = sizeof(SRPWord) - (getTotbytes() & (sizeof(SRPWord)-1));
	}

}

void
CSRPWriteTransaction::complete(BufChain rchn)
{
int     got = rchn->getSize();
SRPWord status;


	if ( got != (int)sizeof(SRPWord)*(nWords_ + expected_) ) {
		if ( got < (int)sizeof(SRPWord)*expected_ ) {
			printf("got %i, nw %i, exp %i\n", got, nWords_, expected_);
			throw IOError("Received message (write response) truncated");
		} else {
			rchn->extract( &status, got - sizeof(status), sizeof(status) );
			if ( needsHdrSwap() )
				swp32( &status );
			throw BadStatusError("SRP Write terminated with bad status", status);
		}
	}

/* We don't really care about the V1 header word; if we did then this is
 * how it could be extracted
	if ( protoVersion_ == IProtoStackBuilder::SRP_UDP_V1 ) {
		rchn->extract( &header, 0, sizeof(header) );
		if ( LE == hostByteOrder() ) {
			swp32( &header );
		}
	} else {
		header = 0;
	}
*/
	rchn->extract( &status, got - sizeof(SRPWord), sizeof(SRPWord) );
	if ( needsHdrSwap() ) {
		swp32( &status );
	}
	if ( status )
		throw BadStatusError("writing SRP", status);
}

void
CSRPReadTransaction::post(ProtoDoor door, unsigned mtu)
{
BufChain xchn = IBufChain::create();

	xchn->insert( xbuf_, 0, sizeof(xbuf_[0])*xbufWords_, mtu );

	door->push( xchn, 0, IProtoPort::REL_TIMEOUT );
}

void
CSRPReadTransaction::complete(BufChain rchn)
{
int	     got = rchn->getSize();
unsigned bufoff;
unsigned i;
int      j;
int	     nWords   = getNWords();

	if ( ! dst_ && sbytes_ ) {
		throw InvalidArgError("CSRPReadTransaction has no destination address");
	}

	iov_[iovLen_].iov_base = rHdrBuf_;
	iov_[iovLen_].iov_len  = hdrWords_ * sizeof(SRPWord) + headBytes_;
	iovLen_++;

	iov_[iovLen_].iov_base = dst_;
	iov_[iovLen_].iov_len  = sbytes_;
	iovLen_++;

	if ( tailBytes_ ) {
		// padding if not word-aligned
		iov_[iovLen_].iov_base = rTailBuf_;
		iov_[iovLen_].iov_len  = tailBytes_;
		iovLen_++;
	}

	iov_[iovLen_].iov_base = &srpStatus_;
	iov_[iovLen_].iov_len  = sizeof(srpStatus_);
	iovLen_++;

	for ( bufoff=0, i=0; i<iovLen_; i++ ) {
		rchn->extract(iov_[i].iov_base, bufoff, iov_[i].iov_len);
		bufoff += iov_[i].iov_len;
	}

#ifdef SRPADDR_DEBUG
	printf("got %i bytes, off 0x%"PRIx64", sbytes %i, nWords %i\n", got, off_, sbytes_, nWords );
	printf("got %i bytes\n", got);
	for (i=0; i<hdrWords_; i++ )
		printf("header[%i]: %x\n", i, rHdrBuf_[i]);
	for ( i=0; i<headBytes_; i++ ) {
		printf("headbyte[%i]: %x\n", i, ((uint8_t*)&rHdrBuf_[hdrWords_])[i]);
	}

#if SRPADDR_DEBUG > 1
	for ( i=0; (unsigned)i<sbytes_; i++ ) {
		printf("chr[%i]: %x %c\n", i, dst_[i], dst_[i]);
	}
#endif

	for ( i=0; i < tailBytes_; i++ ) {
		printf("tailbyte[%i]: %x\n", i, rTailBuf_[i]);
	}
#endif

	if ( got != (int)sizeof(rHdrBuf_[0])*(nWords + expected_) ) {
		if ( got < (int)sizeof(rHdrBuf_[0])*expected_ ) {
			printf("got %i, nw %i, exp %i\n", got, nWords, expected_);
			throw IOError("Received message (read response) truncated");
		} else {
			rchn->extract( &srpStatus_, got - sizeof(srpStatus_), sizeof(srpStatus_) );
			if ( needsHdrSwap() )
				swp32( &srpStatus_ );
			throw BadStatusError("SRP Read terminated with bad status", srpStatus_);
		}
	}

	if ( needsPayloadSwap() ) {
		// switch payload back to LE
		uint8_t  tmp[sizeof(SRPWord)];
		unsigned hoff = 0;
		if ( headBytes_ ) {
			hoff += sizeof(SRPWord) - headBytes_;
			memcpy(tmp, &rHdrBuf_[2], headBytes_);
			if ( hoff > sbytes_ ) {
				// special case where a single word covers rHdrBuf_, dst_ and rTailBuf_
				memcpy(tmp + headBytes_          , dst_     , sbytes_);
				// note: since headBytes < 4 -> hoff < 4
				//       and since sbytes < hoff -> sbytes < 4 -> sbytes == sbytes % 4
				//       then we have: headBytes = off % 4
				//                     totbytes  = off % 4 + sbytes  and totbytes % 4 == (off + sbytes) % 4 == headBytes + sbytes
				//                     tailbytes =  4 - (totbytes % 4)
				//       and thus: headBytes + tailbytes + sbytes  == 4
				memcpy(tmp + headBytes_ + sbytes_, rTailBuf_, tailBytes_);
			} else {
				memcpy(tmp + headBytes_          , dst_     , hoff);
			}
#ifdef SRPADDR_DEBUG
			for (i=0; i< sizeof(SRPWord); i++) printf("headbytes tmp[%i]: %x\n", i, tmp[i]);
#endif
			swpw( tmp );
			memcpy(dst_, tmp+headBytes_, hoff);
		}
		int jend =(((int)(sbytes_ - hoff)) & ~(sizeof(SRPWord)-1));
		for ( j = 0; j < jend; j+=sizeof(SRPWord) ) {
			swpw( dst_ + hoff + j );
		}
		if ( tailBytes_ && (hoff <= sbytes_) ) { // cover the special case mentioned above
			memcpy(tmp, dst_ + hoff + j, sizeof(SRPWord) - tailBytes_);
			memcpy(tmp + sizeof(SRPWord) - tailBytes_, rTailBuf_, tailBytes_);
#ifdef SRPADDR_DEBUG
			for (i=0; i< sizeof(SRPWord); i++) printf("tailbytes tmp[%i]: %"PRIx8"\n", i, tmp[i]);
#endif
			swpw( tmp );
			memcpy(dst_ + hoff + j, tmp, sizeof(SRPWord) - tailBytes_);
		}
#ifdef SRPADDR_DEBUG
		for ( i=0; i< sbytes_; i++ ) printf("swapped dst[%i]: %x\n", i, dst_[i]);
#endif
	}
	if ( needsHdrSwap() ) {
		swp32( &srpStatus_ );
		swp32( &v1Header_  );
	}
	if ( srpStatus_ ) {
		throw BadStatusError("reading SRP", srpStatus_);
	}
}


