#include <cpsw_proto_mod_tdestmux.h>
#include <cpsw_proto_mod_depack.h>

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

	return CByteMuxPort::pushDownstream(bc, rel_timeout);
}

BufChain CTDestPort::processOutput(BufChain bc)
{
Buf b = bc->getHead();

	if ( stripHeader_ ) {
		// add our own
		CAxisFrameHeader hdr;
		hdr.setTDest( getDest() );

		b->adjPayload( - hdr.getSize() );

		hdr.insert( b->getPayload(), b->getSize() );
	} else {
		CAxisFrameHeader::insertTDest( b->getPayload(), b->getSize(), getDest() );
	}

	return bc;
}

int CTDestPort::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	cmp->tDest_.handledBy_ = getProtoMod();
	if ( cmp->tDest_.doMatch_ && static_cast<int>(cmp->tDest_.val_) == getDest() ) {
		cmp->tDest_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	return rval;
}
