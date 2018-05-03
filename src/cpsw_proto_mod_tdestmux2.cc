 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_proto_mod_tdestmux2.h>
#include <cpsw_proto_depack.h>

#include <cpsw_yaml.h>

int CProtoModTDestMux2::extractDest(BufChain bc)
{
Buf b = bc->getHead();

	return CDepack2Header::parseTDest( b->getPayload(), b->getSize() );
}

TDestPort2
CProtoModTDestMux2::addPort(int dest, TDestPort2 port)
{
	CProtoModByteMux<TDestPort2>::addPort(dest, port);
	inputDataAvailable_->add( port->getInputQueueReadEventSource(), this );
	return port;
}

bool CTDestPort2::pushDownstream(BufChain bc, const CTimeout *rel_timeout)
{
	if ( stripHeader_ ) {
		Buf b = bc->getHead();

		uint8_t  *pld = b->getPayload();
		unsigned  sz  = b->getSize();

		unsigned hsize = CDepack2Header::parseHeaderSize( pld, sz );
		unsigned tsize = CDepack2Header::parseTailSize( pld, sz );

		b->adjPayload( hsize );

		b = bc->getTail();

		b->setSize( b->getSize() - tsize );
	}

	return CByteMuxPort<CProtoModTDestMux2>::pushDownstream(bc, rel_timeout);
}


/*
BufChain CTDestPort2::processOutput(BufChain *bcp)
{
BufChain bc = *bcp;
Buf       b = bc->getHead();

	if ( stripHeader_ ) {
		// add our own
		CDepack2Header hdr;
		hdr.setTDest( getDest() );

		b->adjPayload( - hdr.getSize() );

		hdr.insert( b->getPayload(), b->getSize() );

	} else {
		CDepack2Header::insertTDest( b->getPayload(), b->getSize(), getDest() );
	}

	(*bcp).reset();

	return bc;
}
*/

unsigned
CTDestPort2::getMTU()
{
int mtu = mustGetUpstreamDoor()->getMTU();
	// must reserve 1 tail-word for padding
	mtu -= CDepack2Header::size() + 2*CDepack2Header::getTailSize();
	if ( mtu < 0 )
		mtu = 0;
	return (unsigned) mtu;
}

int CTDestPort2::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;
	cmp->tDest_.handledBy_ = getProtoMod();
	if ( cmp->tDest_ == getDest() ) {
		cmp->tDest_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}
	return rval;
}

void
CTDestPort2::dumpYaml(YAML::Node &node) const
{
YAML::Node parms;

	CByteMuxPort<CProtoModTDestMux2>::dumpYaml( node );

	writeNode(parms, YAML_KEY_stripHeader  , stripHeader_   );
	writeNode(parms, YAML_KEY_outQueueDepth, getQueueDepth());
	writeNode(parms, YAML_KEY_TDEST        , getDest()      );

	writeNode(node, YAML_KEY_TDESTMux, parms);
}

bool
CTDestPort2::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
bool rval;

   if ( ! isOpen() )
        return false;

    if ( ! timeout || timeout->isIndefinite() ) {
        rval = inputQueue_->push( bc, NULL );
    } else if ( timeout->isNone() ) {
        rval = inputQueue_->tryPush( bc );
    } else if ( abs_timeout ) {
        rval = inputQueue_->push( bc, timeout );
    } else {
        CTimeout abst( inputQueue_->getAbsTimeoutPush( timeout ) );
        rval = inputQueue_->push( bc, &abst );
    }


	if ( rval ) {
		// keep a count of available buffers in the queue
		inpQueueFill_.fetch_add( 1, memory_order_release );
		return true;
	}

	return false;
}

bool
CTDestPort2::tryPush(BufChain bc)
{
   if ( ! isOpen() )
        return false;

	if ( inputQueue_->tryPush( bc ) ) {
		inpQueueFill_.fetch_add( 1, memory_order_release );
		return true;
	}
	return false;
}


