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
#include <cpsw_crc32_le.h>

// Support for SLAC Depacketizer V2 protocol.
//
// https://confluence.slac.stanford.edu/display/ppareg/AxiStreamPackerizer+Protocol+Version+2
//
// NOTE: This module does not support out-of-order arrival of
//       fragments (like the V0 depacketizer does). The assumption
//       is that reliability and in-order delivery are to be
//       provided by RSSI.

#undef  TDESTMUX2_DEBUG

int CProtoModTDestMux2::extractDest(BufChain bc)
{
Buf b = bc->getHead();

uint8_t tdest = CDepack2Header::parseTDest( b->getPayload(), b->getSize() );

	return tdest;
}

TDestPort2
CProtoModTDestMux2::addPort(int dest, TDestPort2 port)
{
	CProtoModByteMux<TDestPort2>::addPort(dest, port);
	inputDataAvailable_->add( port->getInputQueueReadEventSource(), this );

	work_[numWork_].tdest_       = port->getDest();
	work_[numWork_].stripHeader_ = port->getStripHeader();
	port->attach( numWork_ );
	numWork_++;
	return port;
}

void
CProtoModTDestMux2::modStartup()
{
	CProtoModByteMux<TDestPort2>::modStartup();
	muxer_.threadStart();
}

void
CProtoModTDestMux2::modShutdown()
{
	muxer_.threadStop();
	CProtoModByteMux<TDestPort2>::modShutdown();
}

bool
CProtoModTDestMux2::sendFrag(unsigned current)
{
Work    &w( work_[current] );
BufChain bc = w.bc_;
Buf      h  = bc->getHead();
Buf      t;
bool     eof;
uint8_t  *tailp;
unsigned tailSz;
unsigned algn, newSz;
unsigned numLanes;
unsigned tUsr2;
unsigned added;

	if ( ( eof = ( bc->getSize() <= myMTUCached_ ) ) ) {
		// fits; we can just take over the chain
		w.bc_.reset();
	} else {
		// unlink and put fragment on a new chain
		bc = IBufChain::create();
		added = 0;
		for ( Buf btmp = h; bc->getSize() + btmp->getSize() <= myMTUCached_; btmp = w.bc_->getHead() ) {
#ifdef TDESTMUX2_DEBUG
printf("It %u Chain size %ld, obcs %ld, bufsz %ld\n", added, bc->getSize(), w.bc_->getSize(), btmp->getSize());
#endif
			btmp ->unlink();
			bc->addAtTail( btmp );
			added++;
		}
		if ( 0 == added ) {
			fprintf(stderr,"TDestMux2: bufs %ld, MTU %d\n", (unsigned long)h->getSize(), myMTUCached_);
			throw InternalError("Unable to fragment individual buffers (bufs > MTU)");
		}
	}
#ifdef TDESTMUX2_DEBUG
printf("TDestMux2 sendFrag: %sfragment # %d, size %ld\n", eof ? "last " : "", w.fragNo_, (unsigned long)bc->getSize());
#endif

	CDepack2Header hdr( w.fragNo_, w.tdest_ );

	w.fragNo_++;

	if ( ! w.stripHeader_  &&  w.fragNo_ == 1 ) {
		// let them override just TUSR1 and TID
		try {
			CDepack2Header theirs( h->getPayload(), h->getSize() );
			hdr.setTUsr1( theirs.getTUsr1() );
			hdr.setTId  ( theirs.getTId()   );
		} catch ( CDepack2Header::InvalidHeaderException ) {
			// dump this chain
			w.bc_.reset();
			badHeadersCnt_++;
			// programming error so we throw
			throw;
		}
	} else {
#ifdef TDESTMUX2_DEBUG
		printf("TDestMux2 sendFrag: prepending header\n");
#endif
		h->adjPayload( - hdr.getSize() );
	}

	hdr.insert( h->getPayload(), h->getSize() );

	t = bc->getTail();

	// make the tail
	if ( ! w.stripHeader_ && eof ) {
		// user-provided tail; extract numLanes and tUsr2
		tailp    = t->getPayload() + t->getSize() - CDepack2Header::getTailSize();
		if ( ! CDepack2Header::tailIsAligned( bc->getSize() ) ) {
			// dump
			w.bc_.reset();
			throw CDepack2Header::InvalidHeaderException();
		}
		numLanes = CDepack2Header::parseNumLanes( tailp );
		tUsr2    = CDepack2Header::parseTUsr2( tailp );
	} else {
		newSz    = bc->getSize();
		algn     = CDepack2Header::getTailPadding( bc->getSize() );
		tailSz   = algn + CDepack2Header::getTailSize();
		newSz   += tailSz;

#ifdef TDESTMUX2_DEBUG
printf("TDestMux2 sendFrag: aligning by %d, newSz %u\n", algn, newSz);
#endif

		numLanes = CDepack2Header::ALIGNMENT - algn;

		if ( newSz <= CDepack2Header::getSize() + CDepack2Header::getTailSize() ) {
			// no payload at all!
			numLanes = 0;
		}

        if ( t->getAvail() >= tailSz ) {
			unsigned tmpSz = t->getSize() + tailSz;
			t->setSize( tmpSz );
			tailp = t->getPayload() + tmpSz - CDepack2Header::getTailSize();
		} else {
			t     = bc->createAtTail( IBuf::CAPA_ETH_HDR );
			t->setSize( tailSz );
			tailp = t->getPayload() + algn;
		}
		tUsr2    = 0;
	}

	CDepack2Header::iniTail       ( tailp           );
	CDepack2Header::insertNumLanes( tailp, numLanes );
	CDepack2Header::insertTUsr2   ( tailp, tUsr2    );
	CDepack2Header::insertTailEOF ( tailp, eof      );

	CCpswCrc32LE   crc32;

	CDepack2Header::CrcMode crcMode = hdr.getCrcMode();

	if ( CDepack2Header::NONE != crcMode ) {
		uint8_t       *crcb;
		unsigned long  crcl, crcltot;

		crcb    = h->getPayload();
		// how many bytes need to be CRCed
		crcltot = bc->getSize();
		// how many bytes are in the data buffer
		crcl    = h->getSize();

		if ( crcMode == CDepack2Header::DATA ) {
			// skip header
			crcb    += hdr.getSize();
            crcl    -= hdr.getSize();
			crcltot -= hdr.getSize() + hdr.getTailSize();
		} else {
			// don't include crc itself -- HACK ; we should handle that in CDepack2Header
			crcltot -= sizeof( w.crc_ );
		}

		if ( crcl > crcltot ) {
			crcl = crcltot;
		}

#ifdef TDESTMUX2_DEBUG
		printf("TDestMux2 sendFrag: crc over %ld octets\n", crcltot);
#endif

		w.crc_   = crc32( w.crc_, crcb, crcl );
		crcltot -= crcl;

		while ( crcltot > 0 ) {
			h        = h->getNext();
			crcl     = h->getSize();

			if ( crcl > crcltot ) {
				crcl = crcltot;
			}
			crcb     = h->getPayload();
			w.crc_   = crc32( w.crc_, crcb, crcl );
			crcltot -= crcl;
		}

	} else {
		w.crc_ = 0;
	}


	CDepack2Header::insertCrc( tailp, ~w.crc_  );

	// indefinitely wait (reliability)
	if ( getUpstreamDoor()->push( bc, 0, true ) ) {
		goodTxFragCnt_++;
		if ( eof ) {
			goodTxFramCnt_++;
		}
	}

	return eof;
}

void
CProtoModTDestMux2::process()
{
unsigned current     = 0;
unsigned noWorkCount = 0;

#ifdef TDESTMUX2_DEBUG
	printf("CTDestPort2::muxer started\n");
#endif

	{
	ProtoDoor door = getUpstreamDoor();

	if ( ! door ) {
		throw InternalError("CProtoModTDestMux2::getMTU() -- module not attached");
	}

	myMTUCached_ = getMTU( door );
	}

	while ( 1 ) {
		if ( noWorkCount == numWork_ ) {
			/* found no work; wait for something */
#ifdef TDESTMUX2_DEBUG
			printf("TDestMux2:: going to sleep\n");
#endif
			while ( ! inputDataAvailable_->processEvent( true, NULL ) )
				/* wait */;
#ifdef TDESTMUX2_DEBUG
			printf("TDestMux2:: woke up\n");
#endif
			noWorkCount = 0;
		}
		/* round-robin */
		if ( ++current >= numWork_ ) {
			current = 0;
		}
		if ( work_[current].bc_ ) {
			sendFrag( current );
			noWorkCount = 0;
#ifdef TDESTMUX2_DEBUG
		printf("TDestMux2::work done in slot %d\n", current);
#endif
		} else {
			TDestPort2 p = findPort( work_[current].tdest_ );
			BufChain   bc;
			if ( p && (bc = p->tryPopInputQueue()) ) {
#ifdef TDESTMUX2_DEBUG
				printf("TDestMux2::new work in slot %d (size %ld)\n", current, bc->getSize());
#endif
				work_[current].reset( bc );
				sendFrag( current );
				noWorkCount = 0;
			} else {
#ifdef TDESTMUX2_DEBUG
				printf("TDestMux2::no work for slot %d\n", current);
#endif
				noWorkCount++;
			}
		}
	}
}

bool CTDestPort2::pushDownstream(BufChain bc, const CTimeout *rel_timeout)
{
	try {
		Buf b          = bc->getHead();

		unsigned numLanes;
		bool     eof, sof;

		{
		uint8_t  *pld = b->getPayload();

			CDepack2Header hdr( pld, b->getSize() );

#ifdef TDESTMUX2_DEBUG
			printf("CTDestPort2::pushDownstream; TDEST: 0x%02x, frag %5d\n", hdr.getTDest(), hdr.getFragNo());
#endif

			if ( hdr.getSize() + hdr.getTailSize() > bc->getSize() ) {
				throw CDepack2Header::InvalidHeaderException();
			}

			sof = hdr.isFirst( pld );

			{
				unsigned taillen = hdr.getTailSize();
				Buf      t       = bc->getTail();
				Buf      p;
				unsigned tailsz  = t->getSize();
				unsigned tailcpy = taillen < tailsz ? taillen : tailsz;
				unsigned prevcpy = 0;
				unsigned prevsz;

				uint8_t  tail[taillen];

				memcpy(tail + taillen - tailcpy, t->getPayload() + tailsz - tailcpy, tailcpy);

				if ( (prevcpy =  (taillen - tailcpy)) > 0 ) {
					p      = t->getPrev();
					prevsz = p->getSize();
					if ( prevsz < prevcpy ) {
						throw CDepack2Header::InvalidHeaderException();
					}
					memcpy(tail, t->getPayload() + prevsz - prevcpy, prevcpy);
				}


#ifdef TDESTMUX2_DEBUG
				{
					int i;
					printf("TAIL: 0x");
					for ( i=7; i>=0; i--)
						printf("%02x", tail[i]);
					printf("\n");
				}
#endif

				eof      = CDepack2Header::parseTailEOF ( tail );
				numLanes = CDepack2Header::parseNumLanes( tail );

				// trim header
				if ( stripHeader_ || !sof ) {
					// strip header on non-first segment
					b->adjPayload( CDepack2Header::getSize() );
				}

				// trim tail
				if ( stripHeader_ || !eof ) {

					unsigned strip = 2*taillen - numLanes;

					if ( bc->getSize() < strip ) {
						throw CDepack2Header::InvalidHeaderException();
					}

					while ( strip > 0 ) {
						// Note: t->getSize() might have changed by adjPayload (when stripping header)
						//       if head and tail are in the same buffer!
						tailsz = t->getSize();
						unsigned l = strip > tailsz ? tailsz : strip;
						t->setSize( tailsz - l );
						strip -= l;
						t      = t->getPrev();
					}
				}
			}

			if ( sof ) {
				// just take over the chain
				assembleBuffer_ = bc;
				fragNo_         = 0;
			} else {
				if ( ! assembleBuffer_ || ( (++fragNo_ & ((1<<CDepack2Header::FRAG_NO_BIT_SIZE) - 1)) != hdr.getFragNo() ) ) {
					nonSeqFragCnt_++;
					assembleBuffer_.reset();
					return true;
				}
				while ( b ) {
					Buf next = b->getNext();
					b->unlink(); // from old chain
					assembleBuffer_->addAtTail( b );
					b = next;
				}
			}
		}

		goodRxFragCnt_++;

		// IGNORE CRC

		if ( eof ) {
			goodRxFramCnt_++;
			bc = assembleBuffer_;
			assembleBuffer_.reset();
			return CByteMuxPort<CProtoModTDestMux2>::pushDownstream(bc, rel_timeout);
		}

	} catch (CDepack2Header::InvalidHeaderException) {
		badHeadersCnt_++;
	}
	return true;
}

unsigned CProtoModTDestMux2::getMTU(ProtoDoor upstream)
{
int mtu = upstream->getMTU();
	// must reserve 1 tail-word for padding
	mtu -= CDepack2Header::getSize() + 2*CDepack2Header::getTailSize();
	if ( mtu < 0 )
		mtu = 0;
	return (unsigned) mtu;
}

unsigned
CTDestPort2::getMTU()
{
	return CProtoModTDestMux2::getMTU( mustGetUpstreamDoor() );
}

int CTDestPort2::iMatch(ProtoPortMatchParams *cmp)
{
int rval = 0;

	if ( cmp->tDest_.doMatch_ ) {
		cmp->tDest_.handledBy_  = getProtoMod();
		if ( cmp->tDest_ == getDest() ) {
			cmp->tDest_.matchedBy_ = getSelfAsProtoPort();
			rval++;
		}
	}
	if ( cmp->depackVersion_.doMatch_ ) {
		cmp->depackVersion_.handledBy_ = getProtoMod();
		if ( cmp->depackVersion_ == IProtoStackBuilder::DEPACKETIZER_V2 ) {
			cmp->depackVersion_.matchedBy_ = getSelfAsProtoPort();
			rval++;
		}
	}
	if ( cmp->haveDepack_.doMatch_ ) {
		cmp->haveDepack_.handledBy_ = getProtoMod();
		cmp->haveDepack_.matchedBy_ = getSelfAsProtoPort();
		rval++;
	}

	return rval;
}

void
CTDestPort2::dumpYaml(YAML::Node &node) const
{

	CByteMuxPort<CProtoModTDestMux2>::dumpYaml( node );

	{
	YAML::Node parms;
		writeNode(parms, YAML_KEY_stripHeader  , stripHeader_      );
		writeNode(parms, YAML_KEY_outQueueDepth, getQueueDepth()   );
		writeNode(parms, YAML_KEY_inpQueueDepth, getInpQueueDepth());
		writeNode(parms, YAML_KEY_TDEST        , getDest()         );

		writeNode(node, YAML_KEY_TDESTMux, parms);
	}
	{
	YAML::Node parms;

	IProtoStackBuilder::DepackProtoVersion vers = IProtoStackBuilder::DEPACKETIZER_V2;

		writeNode(parms, YAML_KEY_protocolVersion, vers );

		writeNode(node, YAML_KEY_depack, parms);
	}
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
		getOwner()->postWork( slot_ );
		return true;
	}

	return false;
}

bool
CTDestPort2::tryPush(BufChain bc)
{
   if ( ! isOpen() ) {
        return false;
	}

	if ( inputQueue_->tryPush( bc ) ) {
		getOwner()->postWork( slot_ );
		return true;
	}
	return false;
}

CTDestMuxer2Thread::CTDestMuxer2Thread(CProtoModTDestMux2 *owner, int prio)
: CRunnable( "TDestMuxer2", prio ),
  owner_   ( owner               )
{
}

CTDestMuxer2Thread::CTDestMuxer2Thread(const CTDestMuxer2Thread &orig, CProtoModTDestMux2 *new_owner)
: CRunnable( orig      ),
  owner_   ( new_owner )
{
}

void *
CTDestMuxer2Thread::threadBody()
{
	owner_->process();
	return 0;
}

void
CProtoModTDestMux2::dumpInfo(FILE *f)
{
	CProtoModByteMux<TDestPort2>::dumpInfo(f);
	fprintf(f,"  TX - good fragments      : %10ld\n", goodTxFragCnt_ );
	fprintf(f,"  TX - good frames         : %10ld\n", goodTxFramCnt_ );
}

void
CTDestPort2::dumpInfo(FILE *f)
{
	CByteMuxPort<CProtoModTDestMux2>::dumpInfo( f );
	fprintf(f,"    RX - good fragments    : %10ld\n", goodRxFragCnt_ );
	fprintf(f,"    RX - good frames       : %10ld\n", goodRxFramCnt_ );
	fprintf(f,"    RX - dropped (non-seq) : %10ld\n", nonSeqFragCnt_ );
	fprintf(f,"    RX - dropped (bad-hdr) : %10ld\n", badHeadersCnt_ );
}


