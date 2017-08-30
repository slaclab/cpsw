 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_error.h>
#include <cpsw_thread.h>

#include <cpsw_yaml.h>

#define VC_OFF_V3 7 // v3 is little endian and has an extra word upfront
#define VC_OFF_V2 3 // v2 is little endian
#define VC_OFF_V1 4 // v1 is big endian

BufChain CSRPPort::processOutput(BufChain bc)
{
unsigned off = VC_OFF_V2;

	switch ( getProtoVersion() ) {
		case IProtoStackBuilder::SRP_UDP_V1: off = VC_OFF_V1; break;
		case IProtoStackBuilder::SRP_UDP_V2: off = VC_OFF_V2; break;
		case IProtoStackBuilder::SRP_UDP_V3: off = VC_OFF_V3; break;

		default:
		throw InternalError("CSRPPort::processOutput -- unknown protocol version");
	}

	if ( bc->getSize() <= off ) {
		throw InternalError("CSRPPort::processOutput -- message too small");
	}

	Buf b = bc->getHead();

	while ( b->getSize() <= off ) {
		off -= b->getSize();
		b    = b->getNext();
	}

	// insert virtual channel number;
	*(b->getPayload() + off) = getDest();
	
	return bc;
}

INetIODev::ProtocolVersion CSRPPort::getProtoVersion()
{
	return boost::static_pointer_cast<ProtoModSRPMux::element_type>( getProtoMod() )->getProtoVersion();
}

SRPPort CProtoModSRPMux::newPort(int dest, unsigned queueDepth)
{
	return CShObj::create<SRPPort>( getSelfAs<ProtoModSRPMux>(), dest, queueDepth );
}

int CProtoModSRPMux::extractDest(BufChain bc)
{
uint8_t  vc;
unsigned off = VC_OFF_V2;

	switch ( getProtoVersion() ) {
		case IProtoStackBuilder::SRP_UDP_V1: off = VC_OFF_V1; break;
		case IProtoStackBuilder::SRP_UDP_V2: off = VC_OFF_V2; break;
		case IProtoStackBuilder::SRP_UDP_V3: off = VC_OFF_V3; break;

		default:
		throw InternalError("CSRPPort::processOutput -- unknown protocol version");
	}

	if ( bc->getSize() <= off ) {
		return DEST_MIN-1;
	}

	bc->extract(&vc, off, sizeof(vc));

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

void
CSRPPort::dumpYaml(YAML::Node &node) const
{
YAML::Node parms;

	CByteMuxPort::dumpYaml( parms );

	writeNode(parms, YAML_KEY_virtualChannel, getDest());
	writeNode(parms, YAML_KEY_outQueueDepth,  queueDepth_);

	writeNode(node, YAML_KEY_SRPMux, parms);
}
