 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_DEPACK_H
#define CPSW_PROTO_DEPACK_H

#include <cpsw_error.h>

#include <boost/atomic.hpp>
#include <pthread.h>
#include <stdio.h>

using boost::atomic;
using boost::memory_order_relaxed;

typedef unsigned FrameID;
typedef unsigned FragID;
typedef unsigned ProtoVersion;

// historical name before we knew there would be a 
// (quite different) V2...
class CAxisFrameHeader;
// V2 header
class CPacketizer2Header;

class CPackHeaderUtil {
protected:
	static void     setNum(uint8_t *p, unsigned bit_offset, unsigned bit_size, uint64_t val);
	static uint64_t getNum(uint8_t *p, unsigned bit_offset, unsigned bit_size);

public:
	class InvalidHeaderException {};
};

template <typename T>
class CPackHeaderBase : public CPackHeaderUtil {
private:
	ProtoVersion vers_;
	FragID       fragNo_;
	uint8_t      tDest_;
	uint8_t      tId_;
	uint8_t      tUsr1_;

protected:
	CPackHeaderBase(unsigned fragNo = 0, unsigned tDest = 0)
	:vers_  ( T::VERSION ),
	 fragNo_( fragNo     ),
	 tDest_ ( tDest      ),
	 tId_   ( 0          ),
	 tUsr1_ ( 0          )
	{
		setSOF( fragNo == 0 );
	}


public:
	static const unsigned VERSION_BIT_OFFSET  =  0;
	static const unsigned VERSION_BIT_SIZE    =  4;

	static const unsigned FRAME_NO_BIT_OFFSET = T::FRAME_NO_BIT_OFFSET;
	static const unsigned FRAME_NO_BIT_SIZE   = T::FRAME_NO_BIT_SIZE;
	static const unsigned FRAG_NO_BIT_OFFSET  = T::FRAG_NO_BIT_OFFSET;
	static const unsigned FRAG_NO_BIT_SIZE    = T::FRAG_NO_BIT_SIZE;
	static const unsigned FRAG_MAX_DFLT       = (1<<FRAG_NO_BIT_SIZE) - 1;
	static const unsigned FRAG_MAX            = T::FRAG_MAX_DFLT;

	static const unsigned TDEST_BIT_OFFSET    = T::TDEST_BIT_OFFSET;
	static const unsigned TDEST_BIT_SIZE      = T::TDEST_BIT_SIZE;
	static const unsigned TID_BIT_OFFSET      = T::TID_BIT_OFFSET;
	static const unsigned TID_BIT_SIZE        = T::TID_BIT_SIZE;
	static const unsigned TUSR1_BIT_OFFSET    = T::TUSR1_BIT_OFFSET;
	static const unsigned TUSR1_BIT_SIZE      = T::TUSR1_BIT_SIZE;

	static const unsigned FRAG_FRST_BIT       = T::FRAG_FRST_BIT;
	static const unsigned T_FRAG_LAST_BIT     = T::T_FRAG_LAST_BIT;

	static const unsigned VERSION_0           =  0;
	static const unsigned VERSION_2           =  2;

	static const unsigned VERSION             = T::VERSION;

	static const unsigned HEADER_SIZE         =  8;

	static const unsigned TAIL_SIZE           = T::TAIL_SIZE;

	void         setSOF(bool v)
	{
		if ( v )
			tUsr1_ |=  (1<<FRAG_FRST_BIT);
		else
			tUsr1_ &= ~(1<<FRAG_FRST_BIT);
	}

//	bool parse(uint8_t *hdrBase, size_t hdrSize);

	static unsigned parseVersion(uint8_t *hdrBase, size_t hdrSize)
	{
		return (hdrBase[0] >> VERSION_BIT_OFFSET) & ( (1<<VERSION_BIT_SIZE) - 1 );
	}

	// some quick helpers
	static unsigned parseHeaderSize(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( parseVersion(hdrBase, hdrSize) != VERSION )
			throw InvalidHeaderException();
		return HEADER_SIZE;
	}

	static unsigned parseTailSize(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( parseVersion(hdrBase, hdrSize) != VERSION )
			throw InvalidHeaderException();
		return TAIL_SIZE;
	}


	static unsigned parseTDest(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( parseVersion(hdrBase, hdrSize) != VERSION || hdrSize < HEADER_SIZE )
			throw InvalidHeaderException();
		if ( ( TDEST_BIT_OFFSET % 8 != 0 ) || ( TDEST_BIT_SIZE != 8 ) )
			throw InternalError("Implementation doesn't match header format");
		return hdrBase[ TDEST_BIT_OFFSET/8 ];
	}

	static void insertTDest(uint8_t *hdrBase, size_t hdrSize, uint8_t tdest)
	{
		if ( parseVersion(hdrBase, hdrSize) != VERSION || hdrSize < HEADER_SIZE )
			throw InvalidHeaderException();
		if ( ( TDEST_BIT_OFFSET % 8 != 0 ) || ( TDEST_BIT_SIZE != 8 ) )
			throw InternalError("Implementation doesn't match header format");
		hdrBase[ TDEST_BIT_OFFSET/8 ] = tdest;
	}

	FragID       getFragNo()  { return fragNo_;  }
	ProtoVersion getVersion() { return vers_;    }
	uint8_t      getTDest()   { return tDest_;   }
	uint8_t      getTUsr1()   { return tUsr1_;   }
	uint8_t      getTId()     { return tId_;     }
	bool         getSOF()     { return !! (tUsr1_ & (1<<FRAG_FRST_BIT)); }

	void         setFragNo(FragID   fragNo)
	{
		fragNo_  = fragNo;
		setSOF( fragNo_ == 0 );
	}

	void         setTDest(uint8_t   tDest)
	{
		tDest_   = tDest;
	}

	void         setTUsr1(uint8_t   tUsr1)
	{
		tUsr1_   = tUsr1;
	}

	void         setTId(uint8_t   tId)
	{
		tId_   = tId;
	}

	static size_t getSize()     { return HEADER_SIZE; }

	static size_t getTailSize() { return TAIL_SIZE;   }

	static bool parseTailEOF(uint8_t *tailbuf)
	{
	unsigned idx = T_FRAG_LAST_BIT/8;
	uint8_t  msk = ( 1<< (T_FRAG_LAST_BIT & 7) );
		return tailbuf[idx] & msk;
	}

	static void insertTailEOF(uint8_t *tailbuf, bool eof)
	{
	unsigned idx = T_FRAG_LAST_BIT/8;
	uint8_t  msk = ( 1<< (T_FRAG_LAST_BIT & 7) );
		if ( eof ) {
			tailbuf[idx] |=  msk;
		} else {
			tailbuf[idx] &= ~msk;
		}
	}

	static void iniTail(uint8_t *tailbuf)
	{
		memset(tailbuf, 0, TAIL_SIZE);
	}

	bool
	parse(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( hdrSize  <  (VERSION_BIT_OFFSET + VERSION_BIT_SIZE + 7)/8 ) {
printf("A %d\n", hdrSize);
			return false; // cannot even read the version
		}

		vers_ = getNum( hdrBase, VERSION_BIT_OFFSET, VERSION_BIT_SIZE );

		if ( vers_ != VERSION ) {
printf("b %d\n", vers_);
			return false; // we don't know what size the header would be nor its format
		}

		if ( hdrSize < getSize() ) {
printf("C %d\n", hdrSize);
			return false;
		}

		fragNo_  = getNum( hdrBase, FRAG_NO_BIT_OFFSET,  FRAG_NO_BIT_SIZE  );
		tDest_   = getNum( hdrBase, TDEST_BIT_OFFSET,    TDEST_BIT_SIZE );
		tId_     = getNum( hdrBase, TID_BIT_OFFSET,      TID_BIT_SIZE );
		tUsr1_   = getNum( hdrBase, TUSR1_BIT_OFFSET,    TUSR1_BIT_SIZE );

		return true;
	}

	void
	insert(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( hdrSize < getSize() )
			throw InvalidArgError("Insufficient space for header");
		setNum( hdrBase, VERSION_BIT_OFFSET,  VERSION_BIT_SIZE,  vers_    );
		setNum( hdrBase, FRAG_NO_BIT_OFFSET,  FRAG_NO_BIT_SIZE,  fragNo_  );
		setNum( hdrBase, TDEST_BIT_OFFSET,    TDEST_BIT_SIZE,    tDest_   );
		setNum( hdrBase, TID_BIT_OFFSET,      TID_BIT_SIZE,      tId_     );
		setNum( hdrBase, TUSR1_BIT_OFFSET,    TUSR1_BIT_SIZE,    tUsr1_   );
	}
};

class CAxisFrameHeader : public CPackHeaderBase<CAxisFrameHeader> {
private:
	FrameID      frameNo_;

public:
	static const unsigned FRAME_NO_BIT_OFFSET =  4;
	static const unsigned FRAME_NO_BIT_SIZE   = 12;
	static const unsigned FRAG_NO_BIT_OFFSET  = 16;
	static const unsigned FRAG_NO_BIT_SIZE    = 24;

	static const unsigned TDEST_BIT_OFFSET    = 40;
	static const unsigned TDEST_BIT_SIZE      =  8;
	static const unsigned TID_BIT_OFFSET      = 48;
	static const unsigned TID_BIT_SIZE        =  8;
	static const unsigned TUSR1_BIT_OFFSET    = 56;
	static const unsigned TUSR1_BIT_SIZE      =  8;

	static const unsigned FRAG_FRST_BIT       =  1; // SOF in tUSR1
	static const unsigned T_FRAG_LAST_BIT     =  7; // EOF in tail byte

	static const unsigned VERSION             =  VERSION_0;
	static const unsigned TAIL_SIZE           =  1;

	CAxisFrameHeader(unsigned frameNo = 0, unsigned fragNo = 0, unsigned tDest = 0)
	: CPackHeaderBase<CAxisFrameHeader>(fragNo, tDest),
	  frameNo_(frameNo)
	{
	}

	void insert(uint8_t *hdrBase, size_t hdrSize)
	{
		CPackHeaderBase::insert( hdrBase, hdrSize );
		setNum( hdrBase, FRAME_NO_BIT_OFFSET, FRAME_NO_BIT_SIZE, frameNo_ );
	}

	bool parse(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( ! CPackHeaderBase::parse(hdrBase, hdrSize) )
			return false;
		frameNo_ = getNum( hdrBase, FRAME_NO_BIT_OFFSET, FRAME_NO_BIT_SIZE );
		return true;
	}


	CAxisFrameHeader(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( ! parse(hdrBase, hdrSize) )
			throw InvalidHeaderException();
	}

	class CAxisFrameNoAllocator {
	private:
		atomic<FrameID> frameNo_;
	public:
		CAxisFrameNoAllocator()
		:frameNo_(0)
		{
		}

		FrameID newFrameID()
		{
		FrameID id;
		unsigned shft = (8*sizeof(FrameID)-FRAME_NO_BIT_SIZE);
			id = frameNo_.fetch_add( (1<<shft), memory_order_relaxed ) >> shft;
			return id & ((1<<FRAME_NO_BIT_SIZE) - 1);
		}
	};

	FrameID      getFrameNo() { return frameNo_; }

	void         setFrameNo(FrameID frameNo)
	{
		frameNo_ = frameNo;
	}

	static FrameID moduloFrameSz(FrameID id)
	{
		return (id & ((1<<FRAME_NO_BIT_SIZE) - 1));
	}

	static int     signExtendFrameNo(FrameID x)
	{
		if ( x & (1<<(FRAME_NO_BIT_SIZE-1)) )
			 x |= ~((1<<FRAME_NO_BIT_SIZE)-1);
		return x;
	}

};

class CDepack2Header : public CPackHeaderBase<CDepack2Header> {
public:
	typedef enum { NONE=0, DATA=1, FULL=2 } CrcMode;

private:
	CrcMode crcMode_;
public:

	static const unsigned CRC_MODE_BIT_OFFSET =  4;
	static const unsigned CRC_MODE_BIT_SIZE   =  4;
	static const unsigned TUSR1_BIT_OFFSET    =  8;
	static const unsigned TUSR1_BIT_SIZE      =  8;
	static const unsigned TDEST_BIT_OFFSET    = 16;
	static const unsigned TDEST_BIT_SIZE      =  8;
	static const unsigned TID_BIT_OFFSET      = 24;
	static const unsigned TID_BIT_SIZE        =  8;
	static const unsigned FRAG_NO_BIT_OFFSET  = 32;
	static const unsigned FRAG_NO_BIT_SIZE    = 16;

	static const unsigned T_TUSR2_BIT_OFFSET  =  0;
	static const unsigned T_TUSR2_BIT_SIZE    =  8;
	static const unsigned T_NLANES_BIT_OFFSET = 16;
	static const unsigned T_NLANES_BIT_SIZE   =  4;
	static const unsigned T_CRC_BIT_OFFSET    = 32;
	static const unsigned T_CRC_BIT_SIZE      = 32;

	static const unsigned FRAG_FRST_BIT       =  1; // SOF in tUSR1
	static const unsigned T_FRAG_LAST_BIT     =  8; // EOF in tail byte

	static const unsigned SOF_BIT_OFFSET      = 63; // SOF in V2 header

	static const unsigned VERSION             =  VERSION_2;
	static const unsigned TAIL_SIZE           =  8;

	static const unsigned ALIGNMENT           =  8;

	CDepack2Header(unsigned fragNo = 0, unsigned tDest = 0)
	: CPackHeaderBase<CDepack2Header>(fragNo, tDest),
	  crcMode_( FULL )
	{
	}

	bool
	parse(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( ! CPackHeaderBase::parse(hdrBase, hdrSize) )
			return false;
		crcMode_ = (CrcMode)getNum( hdrBase, CRC_MODE_BIT_OFFSET, CRC_MODE_BIT_SIZE );
		return true;
	}

	void insert(uint8_t *hdrBase, size_t hdrSize)
	{
		CPackHeaderBase::insert( hdrBase, hdrSize );
		// fragNo may roll over (ok for the protocol) but we maintain it as an unsigned
		// so getFragNo() == 0 only marks the first fragment.
		if ( 0 == getFragNo() ) {
			hdrBase[SOF_BIT_OFFSET/8] |= (1<<(SOF_BIT_OFFSET &7));
		}
		setNum( hdrBase, CRC_MODE_BIT_OFFSET, CRC_MODE_BIT_SIZE, (unsigned)crcMode_ );
	}

	CrcMode
	getCrcMode()
	{
		return crcMode_;
	}

	void
	setCrcMode(CrcMode mode)
	{
		crcMode_ = mode;
	}

	bool
	isFirst(uint8_t *hdrBase)
	{
		return hdrBase[SOF_BIT_OFFSET/8] & (1<<(SOF_BIT_OFFSET & 7));
	}

	CDepack2Header(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( ! parse(hdrBase, hdrSize) )
			throw InvalidHeaderException();
	}

	static unsigned parseTUsr2(uint8_t *tailBase)
	{
		return getNum( tailBase, T_TUSR2_BIT_OFFSET, T_TUSR2_BIT_SIZE );
	}

	static void insertTUsr2(uint8_t *tailBase, unsigned val)
	{
		return setNum( tailBase, T_TUSR2_BIT_OFFSET, T_TUSR2_BIT_SIZE, val );
	}


	static unsigned parseNumLanes(uint8_t *tailBase)
	{
	unsigned numLanes = getNum( tailBase, T_NLANES_BIT_OFFSET, T_NLANES_BIT_SIZE );
		if ( numLanes > ALIGNMENT ) {
			throw InvalidHeaderException();
		}
		return numLanes;
	}

	static void insertNumLanes(uint8_t *tailBase, unsigned val)
	{
		return setNum( tailBase, T_NLANES_BIT_OFFSET, T_NLANES_BIT_SIZE, val );
	}


	static uint32_t parseCrc(uint8_t *tailBase)
	{
	uint32_t crc = (uint32_t)getNum( tailBase, T_CRC_BIT_OFFSET, T_CRC_BIT_SIZE );
		/* CRC is stored as BIG-ENDIAN */
		return __builtin_bswap32( crc );
	}

	static void insertCrc(uint8_t *tailBase, uint32_t crc)
	{
		/* CRC is stored as BIG-ENDIAN */
		setNum( tailBase, T_CRC_BIT_OFFSET, T_CRC_BIT_SIZE, __builtin_bswap32( crc ) );
	}

	static unsigned getTailPadding(unsigned size)
	{
		return (ALIGNMENT - size) & (ALIGNMENT - 1);
	}

	static bool tailIsAligned(unsigned size)
	{
		return ! ( size & (ALIGNMENT - 1) );
	}

	static unsigned appendTail(uint8_t *bufBase, unsigned size, bool eof, uint8_t tUsr2 = 0)
	{
		uint8_t *tailBase = bufBase + size;
		unsigned pad      = getTailPadding(size);
		unsigned numLanes = size  > getSize() ? (8 - pad) : 0 /* no payload at all */;

		tailBase         += pad;

		iniTail       (tailBase          );
		insertNumLanes(tailBase, numLanes);
		insertTUsr2   (tailBase, tUsr2   );
		insertTailEOF (tailBase, eof     );
		return pad + getTailSize();
	}

};

#endif
