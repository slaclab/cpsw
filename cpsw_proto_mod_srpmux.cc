#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_error.h>
#include <cpsw_thread.h>

#define VC_OFF_V2 3 // v2 is little endian
#define VC_OFF_V1 4 // v1 is big endian

BufChain CSRPPort::processOutput(BufChain bc)
{
unsigned off = VC_OFF_V2;

	}

	if ( INetIODev::SRP_UDP_V1 == getProtoVersion() )
		off = VC_OFF_V1;

	Buf b = bc->getHead();
	if ( b->getSize() <= off ) {
		throw InternalError("CSRPPort::processOutput -- message too small");
	}

	// insert virtual channel number;
	*(b->getPayload() + off) = getDest();
	
	return bc;
}

INetIODev::ProtocolVersion CSRPPort::getProtoVersion()
{
	return boost::static_pointer_cast<ProtoModSRPMux::element_type>( getProtoMod() )->getProtoVersion();
}

SRPPort CProtoModSRPMux::newPort(int dest)
{
	return CShObj::create<SRPPort>( getSelfAs<ProtoModSRPMux>(), dest );
}

int CProtoModSRPMux::extractDest(BufChain bc)
{
int      vc;
unsigned off = VC_OFF_V2;


	if ( INetIODev::SRP_UDP_V1 == getProtoVersion() )
		off = VC_OFF_V1;

	Buf b = bc->getHead();
	if ( b->getSize() <= off ) {
		return DEST_MIN-1;
	}

	vc = *(b->getPayload() + off);

	if ( vc > DEST_MAX ) {
		return DEST_MIN - 1;
	}

	return vc;
}

int CSRPPort::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	cmp->srpVersion_.handledBy_ = getProtoMod();
	if ( cmp->srpVersion_ == getProtoVersion() ) {
		cmp->srpVersion_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	cmp->srpVC_.handledBy_ = getProtoMod();
	if ( cmp->srpVC_ == getDest() ) {
		cmp->srpVC_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	return rval;
}
