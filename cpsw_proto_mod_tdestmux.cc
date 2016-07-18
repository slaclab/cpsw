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
Buf    b = bc->getHead();
size_t new_sz;

	if ( stripHeader_ ) {
		// add our own
		CAxisFrameHeader hdr;
		hdr.setTDest( getDest() );

		b->adjPayload( - hdr.getSize() );

		hdr.insert( b->getPayload(), b->getSize() );

		// append tail
		b = bc->getTail();

		new_sz = b->getSize() + hdr.getTailSize();
		if ( b->getAvail() >= new_sz ) {
			b->setSize( new_sz );
		} else {
			b = bc->createAtTail( IBuf::CAPA_ETH_HDR );
			b->setSize( hdr.getTailSize() );
		}

		hdr.iniTail( b->getPayload() + b->getSize() - hdr.getTailSize() );
	} else {
		CAxisFrameHeader::insertTDest( b->getPayload(), b->getSize(), getDest() );
	}

	return bc;
}

int CTDestPort::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	cmp->tDest_.handledBy_ = getProtoMod();
	if ( cmp->tDest_ == getDest() ) {
		cmp->tDest_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	return rval;
}
