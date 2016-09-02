 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_RSSI_PROTO_H
#define CPSW_RSSI_PROTO_H

#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <stdio.h>

class RssiHeader {
public:
	const static uint8_t RSSI_VERSION_1 = 1;

	const static uint8_t FLG_BSY = (1<<0);
	const static uint8_t FLG_NUL = (1<<3);
	const static uint8_t FLG_RST = (1<<4);
	const static uint8_t FLG_EAC = (1<<5);
	const static uint8_t FLG_ACK = (1<<6);
	const static uint8_t FLG_SYN = (1<<7);

	typedef enum { READ, SET } MODE;

	class BadHeader {
	public:
		std::string msg_;
		BadHeader( const char *msg ) : msg_(msg) {}
	};

protected:
	uint8_t  * buf_;
	bool     chkOk_;

	// buffer size must already have been verified
	uint16_t cs()
	{
	uint32_t cs_i = 0;
	unsigned    i;

		for ( i=0; i < getHSize(); i+=2 ) {
			cs_i += ld16be( i );
		}
		while ( cs_i > 0xffff ) {
			cs_i = (cs_i & 0xffff) + (cs_i >> 16);
		}
		return (uint16_t)( (~cs_i) & 0xffff );
	}

	void st16be(unsigned off, uint16_t v)
	{
		buf_[off+0] = v >> 8;
		buf_[off+1] = v;
	}

	uint16_t ld16be(unsigned off)
	{
		return (buf_[off+0] << 8 ) | buf_[off+1];
	}

	void st32be(unsigned off, uint32_t v)
	{
		buf_[off+0] = v >> 24;
		buf_[off+1] = v >> 16;
		buf_[off+2] = v >>  8;
		buf_[off+3] = v >>  0;
	}

	uint32_t ld32be(unsigned off)
	{
		return (((( (buf_[off+0] << 8) | buf_[off+1] ) << 8 ) | buf_[off+2]) << 8 ) | buf_[off+3];
	}

	RssiHeader(uint8_t *buf, size_t bufsz, bool computeChksum, uint8_t hLen);

public:
	// Unchecked constructor - must only be used on an outgoing header
	// that has been pre-written but needs the ack-no or checksum
	// modified...
	// chkOk_ is undefined
	RssiHeader(uint8_t *buf)
	:buf_(buf)
	{
	}

	RssiHeader(uint8_t *buf, size_t bufsz, bool computeChksum, MODE initMode);

	uint8_t  getFlags() { return buf_[0]; }
	uint8_t  getHSize() { return buf_[1]; }
	// use uint16_t for seq numbers (future enhancement?)
	uint16_t getSeqNo() { return buf_[2]; }
	uint16_t getAckNo() { return buf_[3]; }
	uint16_t getCheck() { return ld16be( getHSize() - 2 ); }

	bool     getChkOk() { return chkOk_; }

	void     setFlags(uint8_t  v) { buf_[0] = v; }
	void     setHSize(uint8_t  v) { buf_[1] = v; }
	void     setSeqNo(uint16_t v) { buf_[2] = static_cast<uint8_t>(v); }
	void     setAckNo(uint16_t v) { buf_[3] = static_cast<uint8_t>(v); }
	void     setCheck(uint16_t v) { st16be( getHSize() - 2, v );       }

	// buffer size must already have been verified
	void writeChksum()
	{
		st16be( getHSize() - 2, 0 );
		st16be( getHSize() - 2, cs() );
	}

	bool isPureAck()
	{
		return (getFlags() & ~FLG_BSY) == FLG_ACK;
	}

	// hasPayload: > 0 yes, == 0 no, < 0 unknown
	void dump(FILE *f, int hasPayload = -1);

	// static helpers for frequent operations
	// assume buffer has been validated already

	static uint8_t getFlags(uint8_t *buf)
	{
		return buf[0];
	}

	static uint8_t getHSize(uint8_t *buf)
	{
		return buf[1];
	}

	static unsigned minHeaderSize()
	{
		return 8;
	}

	static uint8_t getHSize(uint8_t protoVersion);

	virtual ~RssiHeader() {}

	static uint8_t getSupportedVersion()
	{
		return RSSI_VERSION_1;
	}

};

class RssiSynHeader : public RssiHeader {
public:

	const static uint8_t XFL_CHK = (1<<2);
	const static uint8_t XFL_ONE = (1<<3);



	uint8_t  getXflgs() { return    buf_[ 4];  }
	 // max unacked segments;    NON_NEGO -- each side accepts peer's value.
	uint8_t  getOssMX() { return    buf_[ 5];  }
	 // max seg size (inc hdr);  NON_NEGO -- each side accepts peer's value.
	uint16_t getSgsMX() { return ld16be(  6 ); }
	 // rexmit timeout;          NEGOTIATED
	uint16_t getRexTO() { return ld16be(  8 ); }
	 // cumulative ack timeout;  NEGOTIATED  < rexTO
	uint16_t getCakTO() { return ld16be( 10 ); }
	 // nul-timeout;             NEGOTIATED  (0 disables)
	uint16_t getNulTO() { return ld16be( 12 ); }
	 // max # retrans;           NEGOTIATED (0 -> forever)
	uint8_t  getRexMX() { return    buf_[14];  }
	 // max # accumulated ack;   NEGOTIATED (0 -> ack immediately)
	uint8_t  getCakMX() { return    buf_[15];  }
	 // max out of seq for EACK; NEGOTIATED (0 -> immediate EACK)
	uint8_t  getOsaMX() { return    buf_[16];  }
	uint8_t  getUnits() { return    buf_[17];  }
	 // provided by server :-(
	uint32_t getConID() { return ld32be( 18 ); }
	 // unique for each side; for auto-reset
	uint8_t  getVersn() { return getXflgs() >> 4; }

	void setXflgs(uint8_t  v) { buf_[   4] = v; }
	void setOssMX(uint8_t  v) { buf_[   5] = v; }
	void setSgsMX(uint16_t v) { st16be( 6, v);  }
	void setRexTO(uint16_t v) { st16be( 8, v);  }
	void setCakTO(uint16_t v) { st16be(10, v);  }
	void setNulTO(uint16_t v) { st16be(12, v);  }
	void setRexMX(uint8_t  v) { buf_[  14] = v; }
	void setCakMX(uint8_t  v) { buf_[  15] = v; }
	void setOsaMX(uint8_t  v) { buf_[  16] = v; }
	void setUnits(uint8_t  v) { buf_[  17] = v; }
	void setConID(uint32_t v) { st32be(18, v);  }

	void setVersn(uint8_t  v)
	{
	uint8_t prev = getXflgs();
		prev &= 0xf;
		setXflgs( prev | (v << 4) );
	}

	RssiSynHeader(uint8_t *buf, size_t bufsz, bool computeChksum, MODE initMode);
	RssiSynHeader();

	virtual ~RssiSynHeader() {}
};

#endif
