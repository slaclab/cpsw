
#include <cpsw_error.h>
#include <cpsw_rssi_proto.h>



RssiHeader::RssiHeader(uint8_t *buf, size_t bufsz, bool computeChksum, uint8_t hLen)
:buf_(buf)
{

	if ( bufsz < 2 || bufsz < ( hLen ? (buf[1]=hLen) : buf[1] ) ) {
		throw BadHeader("header size > buffer size ?");
	}

	if ( computeChksum ) {
		if ( hLen ) {
			chkOk_ = false;
		} else {
			chkOk_ = ( cs() == 0 );
		}
	} else {
		chkOk_ = true;
	}
}

RssiHeader::RssiHeader(uint8_t *buf, size_t bufsz, bool computeChksum, MODE initMode)
:buf_(buf)
{

	if ( bufsz < 2 || bufsz <  ( SET == initMode ? (buf[1]=8) : buf[1] ) ) {
		throw BadHeader("header size > buffer size ?");
	}

	if ( computeChksum ) {
		if ( SET == initMode ) {
			chkOk_ = false;
		} else {
			chkOk_ = ( cs() == 0 );
		}
	} else {
		chkOk_ = true;
	}
}

RssiSynHeader::RssiSynHeader(uint8_t *buf, size_t bufsz, bool computeChksum, MODE initMode)
: RssiHeader(buf, bufsz, false, (SET == initMode ? (uint8_t)24 : (uint8_t)0 ) )
{
	if ( SET != initMode ) {
		if ( 24 != bufsz && 24 != getHSize() ) {
			throw BadHeader("SYN must not have data");
		}
		if ( getVersn() != RSSI_VERSION_1 ) {
			throw BadHeader("Invalid Protocol Version");
		}
	}
	if ( computeChksum ) {
		if ( SET == initMode ) {
			chkOk_ = false;
		} else {
			chkOk_ = ( cs() == 0 );
		}
	} else {
		chkOk_ = true;
	}
}
	
uint8_t RssiHeader::getHSize(uint8_t protoVersion)
{
	if ( RssiSynHeader::RSSI_VERSION_1 != protoVersion )
		throw InternalError("Unknown RSSI Version");
	return 8;
}

void RssiHeader::dump(FILE *f, int hasPayload)
{
char      pld = hasPayload > 0 ? 'P' : hasPayload < 0 ? '?' : '-';
uint8_t flags = getFlags();

		fprintf(f,"SEQ %d, ACK %d [%c%c%c%c%c%c]",
			getSeqNo(),
			getAckNo(),
			(flags & RssiHeader::FLG_SYN) ? 'S':'-',
			(flags & RssiHeader::FLG_ACK) ? 'A':'-',
			(flags & RssiHeader::FLG_BSY) ? 'B':'-',
			(flags & RssiHeader::FLG_RST) ? 'R':'-',
			(flags & RssiHeader::FLG_NUL) ? 'N':'-',
			pld);
}
