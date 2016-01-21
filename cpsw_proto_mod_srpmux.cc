#include <cpsw_proto_mod_srpmux.h>

#define VC_OFF_V2 3 // little endian
#define VC_OFF_V1 4 // big endian


SRPPort CProtoModSRPMux::createPort(int vc)
{

	if ( vc < VC_MIN || vc > VC_MAX )
		throw InvalidArgError("Virtual channel number out of range");
	if ( ! downstream_[vc].expired() ) {
		throw ConfigurationError("Virtual channel already in use");
	}

	SRPPort port = CShObj::create<SRPPort>( getSelfAs<ProtoModSRPMux>(), vc );

	downstream_[vc - VC_MIN] = port;

	return port;
}

BufChain CSRPPort::processOutput(BufChain bc)
{
unsigned off = VC_OFF_V2;
	if ( bc->getLen() != 1 ) {
		throw InternalError("CSRPPort::processOutput -- expect only 1 buffer");
	}

	if ( INoSsiDev::SRP_UDP_V1 == getProtoVersion() )
		off = VC_OFF_V1;

	Buf b = bc->getHead();
	if ( b->getSize() <= off ) {
		throw InternalError("CSRPPort::processOutput -- message too small");
	}

	// insert virtual channel number;
	*(b->getPayload() + off) = getVC();
	
	return bc;
}

bool CProtoModSRPMux::pushDown(BufChain bc, const CTimeout *rel_timeout)
{
int      vc;
unsigned off = VC_OFF_V2;

	if ( bc->getLen() != 1 ) {
		return false;
	}

	if ( INoSsiDev::SRP_UDP_V1 == getProtoVersion() )
		off = VC_OFF_V1;

	Buf b = bc->getHead();
	if ( b->getSize() <= off ) {
		return false;
	}

	vc = *(b->getPayload() + off);

	if ( vc > VC_MAX || vc < VC_MIN ) {
		return false;
	}

	if ( downstream_[vc].expired() )
		return false; // nothing attached to this port; drop

	return SRPPort( downstream_[vc] )->pushDownstream(bc, rel_timeout);
}

int CSRPPort::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	if ( cmp->srpVersion_.doMatch_ && static_cast<INoSsiDev::ProtocolVersion>(cmp->srpVersion_.val_) == getProtoVersion() ) {
		cmp->srpVersion_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	if ( cmp->srpVC_.doMatch_ && static_cast<int>(cmp->srpVC_.val_) == getVC() ) {
		cmp->srpVC_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	return rval;
}
