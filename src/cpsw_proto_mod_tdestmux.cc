 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_proto_mod_tdestmux.h>
#include <cpsw_proto_depack.h>

#include <cpsw_yaml.h>

int CProtoModTDestMux::extractDest(BufChain bc)
{
Buf b = bc->getHead();

	return CAxisFrameHeader::parseTDest( b->getPayload(), b->getSize() );
}

bool CTDestPort::pushDownstream(BufChain bc, const CTimeout *rel_timeout)
{
	if ( stripHeader_ ) {
		Buf b = bc->getHead();

		uint8_t  *pld = b->getPayload();
		unsigned  sz  = b->getSize();

		unsigned hsize = CAxisFrameHeader::parseHeaderSize( pld, sz );
		unsigned tsize = CAxisFrameHeader::parseTailSize( pld, sz );

		b->adjPayload( hsize );

		b = bc->getTail();

		b->setSize( b->getSize() - tsize );
	}

	return CByteMuxPort<CProtoModTDestMux>::pushDownstream(bc, rel_timeout);
}

BufChain CTDestPort::processOutput(BufChain *bcp)
{
BufChain bc = *bcp;
Buf       b = bc->getHead();

	if ( stripHeader_ ) {
		// add our own
		CAxisFrameHeader hdr;
		hdr.setTDest( getDest() );

		b->adjPayload( - hdr.getSize() );

		hdr.insert( b->getPayload(), b->getSize() );

	} else {
		CAxisFrameHeader::insertTDest( b->getPayload(), b->getSize(), getDest() );
	}

	(*bcp).reset();

	return bc;
}

int CTDestPort::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	cmp->tDest_.handledBy_ = getProtoMod();
	if ( cmp->tDest_ == getDest() && cmp->depackVersion_ == IProtoStackBuilder::DEPACKETIZER_V0 ) {
		cmp->tDest_.matchedBy_ = getSelfAsProtoPort();
		rval+=1;
	}
	return rval;
}

void
CTDestPort::dumpYaml(YAML::Node &node) const
{
YAML::Node parms;

	CByteMuxPort<CProtoModTDestMux>::dumpYaml( node );

	writeNode(parms, YAML_KEY_stripHeader  , stripHeader_   );
	writeNode(parms, YAML_KEY_outQueueDepth, getQueueDepth());
	writeNode(parms, YAML_KEY_TDEST        , getDest()      );

	writeNode(node, YAML_KEY_TDESTMux, parms);
}
