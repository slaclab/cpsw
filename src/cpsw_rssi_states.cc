 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_rssi.h>

#include <stdio.h>

// Default handlers panic
void CRssi::STATE::handleRxEvent(CRssi *context, IIntEventSource *src)
{
	throw InternalError("RxEvent unexpected in this state");
}

void CRssi::STATE::handleUsrInputEvent(CRssi *context, IIntEventSource *src)
{
	throw InternalError("UsrInputEvent unexpected in this state");
}

void CRssi::STATE::handleUsrOutputEvent(CRssi *context, IIntEventSource *src)
{
	throw InternalError("UsrOutputEvent unexpected in this state");
}

void CRssi::STATE::processRetransmissionTimeout(CRssi *context)
{
	throw InternalError("RETRANSMISSIONTimeout unexpected in this state");
}

void CRssi::STATE::processAckTimeout(CRssi *context)
{
	throw InternalError("ACKTimeout unexpected in this state");
}

void CRssi::STATE::processNulTimeout(CRssi *context)
{
	throw InternalError("NULTimeout unexpected in this state");
}

void CRssi::STATE::shutdown(CRssi *context)
{
}

void CRssi::LISTEN::shutdown(CRssi *context)
{
}

void CRssi::NOTCLOSED::shutdown(CRssi *context)
{
	context->sendRST();
}

int CRssi::STATE::getConnectionState(CRssi *context)
{
	return 0;
}

int CRssi::CLOSED::getConnectionState(CRssi *context)
{
	return CONN_STATE_CLOSED;
}

int CRssi::OPEN::getConnectionState(CRssi *context)
{
	return CONN_STATE_OPEN;
}

void CRssi::CLOSED::advance(CRssi *context)
{
	context->close();
	context->open();

	if ( context->isServer_ ) {
		context->changeState( &context->stateLISTEN );
	} else {
		if ( context->closedReopenDelay_.tv_sec ) {
			fprintf(stderr,"Client unable to establish connection; sleeping for a while\n");
			::nanosleep( &context->closedReopenDelay_, NULL );
		}
		if ( context->closedReopenDelay_.tv_sec < 10 ) {
			context->closedReopenDelay_.tv_sec++;
		}
		context->sendSYN( false );
		context->changeState( &context->stateCLNT_WAIT_SYN_ACK );
	}
}

void CRssi::NOTCLOSED::advance(CRssi *context, bool checkTimer)
{
RssiTimer *timer = context->timers_.getFirstToExpire();

	if ( ! timer && checkTimer ) {
		throw InternalError("RSSI: no timer active!");
	}
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
	fprintf(stderr, "%s: scheduling timer %s (state %s)\n", context->getName(), timer ? timer->getName() : "<NONE>", getName());
}
#endif
	if ( ! context->eventSet_->processEvent(true, timer ? timer->getTimeout() : NULL ) ) {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
		fprintf(stderr, "%s: canceling+processing timer %s (state %s)\n", context->getName(), timer ? timer->getName() : "<NONE>", getName());
}
#endif
		timer->cancel();
		timer->process();
	}
}

void CRssi::NOTCLOSED::advance(CRssi *context)
{
	advance(context, true);
}

void CRssi::NOTCLOSED::drainReassembleBuffer(CRssi *context)
{
	// unless in OPEN state - do nothing
}

void CRssi::NOTCLOSED::handleRxEvent(CRssi *context, IIntEventSource *src)
{
BufChain bc;
uint8_t  flags;
bool     hasPayload;

	if ( ! (bc = context->tryPopUpstream()) ) {
		fprintf(stderr, "%s: SRC pending %d\n", context->getName(), src->isPending());
		throw InternalError("Spurious wakeup ??");
	}

	if ( bc->getSize() < RssiHeader::minHeaderSize() ) {
		// DROP
		return;
	}

	try {
		Buf b = bc->getHead();

		RssiHeader hdr( b->getPayload(), b->getSize(), context->verifyChecksum_, RssiHeader::READ );

		if ( ! hdr.getChkOk() ) {
			fprintf(stderr, "%s dropped, bad checksum\n", getName());
			context->stats_.badChecksum_++;
			// DROP
			return;
		}

		flags = RssiHeader::getFlags( b->getPayload() );
		hasPayload = b->getSize() > hdr.getHSize();

#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 0 )
{
fprintf(stderr,"RX: %s -- ", context->getName());
hdr.dump( stderr, hasPayload ? 1 : 0 );
fprintf(stderr," (state %s)\n", getName());
}
#endif

		if ( (flags & RssiHeader::FLG_SYN) ) {
			try {
				RssiSynHeader synHdr( b->getPayload(), b->getSize(), false /* already verified */, RssiHeader::READ );
				if ( ! handleSYN( context, synHdr ) ) {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr,"%s syn rejected (state %s)\n", context->getName(), getName());
}
#endif
					return;
				}
			} catch ( RssiHeader::BadHeader ) {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr, "%s: dropped (bad header)\n", context->getName());
}
#endif
				context->stats_.badSynDropped_++;
				return;
			}
		} else {
			if ( ! handleOTH( context, hdr, hasPayload ) ) {
				context->stats_.rejectedSegs_++;
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr,"%s OTH rejected (state %s)\n", context->getName(), getName());
}
#endif
				return;
			}
		}

		// clean out our outgoing buffer
		context->processAckNumber( hdr.getFlags(), hdr.getAckNo() );

		// cache the busy flag (header no longer valid further down);
		bool peerNowBSY = !! (hdr.getFlags() & RssiHeader::FLG_BSY);

		if ( hasPayload || (flags & RssiHeader::FLG_NUL) ) {

			if ( context->unOrderedSegs_.canAccept( hdr.getSeqNo() ) ) {

#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"RX: %s  storing (oldest %d)", context->getName(), context->unOrderedSegs_.getOldest());
hdr.dump(stderr, b->getSize() > hdr.getHSize());
fprintf(stderr," (state %s)\n", getName());
}
#endif

				// AFTER THIS SEQUENCE OF OPERATIONS WE NO LONGER OWN THE BUFFER
				// NOR THE INCLUDED HEADER, I.E., MUST CACHE HEADER VALUES USED
				// THEREAFTER!
				b.reset();
				context->unOrderedSegs_.store( hdr.getSeqNo(), bc );

				drainReassembleBuffer( context );

			} else {
				// should not happen if the peer respects our max. unacked window
				// but still could as a result of retransmissions...
				context->stats_.rejectedSegs_++;

				// still send ACK if necessary
			}

		}

		bool peerWasBSY   = context->peerBSY_;
		context->peerBSY_ = peerNowBSY;

		if ( context->peerBSY_ ) {
			context->stats_.busyFlagsCountedRx_++;
			if ( ! peerWasBSY )
				context->rexTimer()->cancel();
		}

#if 1
		if ( peerWasBSY && ! context->peerBSY_ && context->unAckedSegs_.getSize() > 0 ) {
			context->stats_.busyDeassertRex_++;
			processRetransmissionTimeout( context );
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr,"%s: Retransmission due to peerBSY -> !peerBSY (state %s)\n", context->getName(), getName());
}
#endif
			
			// retransmitting sends ACK and arms the NUL timer; thus we're done.
			return;
		}
#endif

		// do we have to ACK this one?
		++context->numCak_;
		if ( context->numCak_ > context->cakMX_ ) {
			BufChain b1 = hasBufToSend(context);
			if ( b1 )
				context->sendDAT( b1 );
			else
				context->sendACK();
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"%s: cakMX reached; sent %c (state %s)\n", context->getName(), b1 ? 'B':'A', getName());
}
#endif
		} else if ( 1 == context->numCak_ ) {
			// if SYN/ACK was sent in server mode then we don't want to use the timer
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"%s: arming ACK timer (state %s)\n", context->getName(), getName());
}
#endif
			context->ackTimer()->arm_rel( context->cakTO_ );
		} else {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"%s: skipping ACK (numCak: %d - max %d) (state %s)\n", context->getName(), context->numCak_, context->cakMX_, getName());
}
#endif
		}

		// let anything acceptable -- which shows the peer is alive --
		// reset the NUL timer
		if ( context->isServer_ )
			context->nulTimer()->arm_rel( context->nulTO_ );

	} catch ( RssiHeader::BadHeader ) {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr, "%s: dropped (bad header)\n", context->getName());
}
#endif
		context->stats_.badHdrDropped_++;
	}
}

void CRssi::SERV_WAIT_SYN_ACK::handleRxEvent(CRssi *context, IIntEventSource *src)
{
	CRssi::NOTCLOSED::handleRxEvent(context, src);
	if ( context->unAckedSegs_.getSize() == 0 ) {
		context->changeState( &context->statePREP_OPEN );
		context->nulTimer()->arm_rel( context->nulTO_ );
	}
}

void CRssi::WAIT_SYN::extractConnectionParams(CRssi *context, RssiSynHeader &synHdr)
{
	context->peerOssMX_      = synHdr.getOssMX();

#ifdef RSSI_DEBUG
if ( cpsw_rssi_debug > 0 ) {
	fprintf(stderr, "RSSI Peer OSS Max: %d\n", synHdr.getOssMX());
}
#endif

	context->peerSgsMX_      = synHdr.getSgsMX();

#ifdef RSSI_DEBUG
if ( cpsw_rssi_debug > 0 ) {
	fprintf(stderr, "RSSI Peer SGS Max: %d\n", synHdr.getSgsMX());
}
#endif

	if ( context->peerOssMX_ > context->unAckedSegs_.getCapa() )
		context->unAckedSegs_.resize( context->peerOssMX_ );
}

void CRssi::CLNT_WAIT_SYN_ACK::extractConnectionParams(CRssi *context, RssiSynHeader &synHdr)
{
	WAIT_SYN::extractConnectionParams( context, synHdr );

	uint8_t  unitExp;
	unsigned i, u;

	// accept server's parameters
	context->verifyChecksum_ = context->addChecksum_ = !! (synHdr.getXflgs() & RssiSynHeader::XFL_CHK);

	// our units are US
	if ( (unitExp = synHdr.getUnits()) > 6 )
		throw InternalError("Cannot handle time units less than 1us");

	for ( i=0, u=1; i < (unsigned)6 - unitExp; i++ )
		u *= 10;

	context->units_ = u;

	context->rexTO_ = CTimeout(  synHdr.getRexTO() * u );
	context->cakTO_ = CTimeout(  synHdr.getCakTO() * u );
	context->nulTO_ = CTimeout( (synHdr.getNulTO() * u ) / 3 );
	context->rexMX_ = synHdr.getRexMX();
	context->cakMX_ = synHdr.getCakMX();
}

bool CRssi::NOTCLOSED::handleSYN(CRssi *context, RssiSynHeader &synHdr)
{
	// default: just accept a valid SYN

	// a client could also be here in OPEN state if the
	// server didn't receive the ACK to its SYN message.
	// In this case the client is OPEN and receives SYN

	context->forceACK();

	return true;
}

bool CRssi::LISTEN::handleSYN(CRssi *context, RssiSynHeader &synHdr)
{
#ifdef RSSI_DEBUG
if ( cpsw_rssi_debug > 0 ) {
		fprintf(stderr, "%s SYN received, good checksum (state %s)\n", context->getName(), getName());
}
#endif

		extractConnectionParams( context, synHdr );

		context->lastSeqRecv_ = synHdr.getSeqNo();
		context->unOrderedSegs_.seed( context->unOrderedSegs_.nxtSeqNo( context->lastSeqRecv_ ) );

		context->sendSYN( true /* ACK */ );
		// since we already sent and ACK we decrement the numCak_ counter
		context->numCak_--;

		context->changeState( &context->stateSERV_WAIT_SYN_ACK );

		// OK RETURN
		return true;
}

bool CRssi::CLNT_WAIT_SYN_ACK::handleSYN(CRssi *context, RssiSynHeader &synHdr)
{
#ifdef RSSI_DEBUG
if ( cpsw_rssi_debug > 0 ) {
		fprintf(stderr, "%s SYN received, good checksum (state %s)\n", context->getName(), getName());
}
#endif

		extractConnectionParams( context, synHdr );

		if (  ! (RssiHeader::FLG_ACK & synHdr.getFlags()) ) {
			// another client is trying to contact us;
			fprintf(stderr,"Another client is trying to open a connection to US\n");
			context->sendRST();
			context->changeState( &context->stateCLOSED );
			return false;
		}

		context->lastSeqRecv_ = synHdr.getSeqNo();
		context->unOrderedSegs_.seed( context->unOrderedSegs_.nxtSeqNo( context->lastSeqRecv_ ) );

		context->forceACK();

		if ( synHdr.getAckNo() == context->lastSeqSent_ ) {
			context->changeState( &context->statePREP_OPEN );
		}

		// OK RETURN
		return true;
}

bool CRssi::NOTCLOSED::handleOTH(CRssi *context, RssiHeader &hdr, bool hasPayload)
{

	if ( hdr.isPureAck() && ! hasPayload ) {
		// pure ACK uses LAST seq no;
		// whereas 'canAccept()' below expects the next one.

		// suppress an ACK to this one
		context->numCak_--;
		return true;
	}

	if ( (hdr.getFlags() & RssiHeader::FLG_NUL) ) {
		if ( context->isServer_ && ! hasPayload ) {
			context->forceACK();
			return true;
		}
	} else if ( hdr.getFlags() & RssiHeader::FLG_RST ) {
		context->changeState( &context->stateCLOSED );
		return true;
	} else if ( hasPayload ) {
		return true;
	}
	return false;
}

bool CRssi::WAIT_SYN::handleOTH(CRssi *context, RssiHeader &hdr, bool hasPayload)
{
	// dont' accept other headers except RST
	if ( (hdr.getFlags() & RssiHeader::FLG_RST) && context->unOrderedSegs_.canAccept( hdr.getSeqNo() ) && ! hasPayload ) {
		context->sendRST();
		context->changeState( &context->stateCLOSED );
		return true;
	}
	return false;
}


void CRssi::LISTEN::advance(CRssi *context)
{
	CRssi::NOTCLOSED::advance(context, false);
}

void CRssi::NOTCLOSED::processRetransmissionTimeout(CRssi *context)
{


	if ( ++context->numRex_ > context->rexMX_ ) {
		fprintf(stderr,"%s: Connection Failed (max retransmissions exceeded)\n", getName());
		context->stats_.connFailed_++;
		context->sendRST();
		context->changeState( &context->stateCLOSED );
		return;
	}

	CRssi::RingBuf::iterator it = context->unAckedSegs_.begin();
	BufChain bc;

	if ( (bc = *it) ) {
		do {
			context->stats_.rexSegments_++;
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 2 )
{
fprintf(stderr,"%s: retransmitting (state %s)\n", getName(), context->getName());
}
#endif
			context->sendBuf(bc, true);	
		} while ( (bc = *(++it)) );
		context->armRexAndNulTimer();
	}
}

void CRssi::OPEN::processNulTimeout(CRssi *context)
{
	if ( context->isServer_ ) {
		context->stats_.connFailed_++;
		context->sendRST();
		context->changeState( &context->stateCLOSED );
		fprintf(stderr,"%s: connection failed: server side closing! (state %s)\n", context->getName(), getName());
	} else {
		if ( ! context->sendNUL() ) {
			// no space in window; disable input events
			context->usrIEH()->disable();
			context->changeState( &context->stateOPEN_OUTWIN_FULL );
		}
	}
}

void CRssi::OPEN::handleUsrInputEvent(CRssi *context, IIntEventSource *src)
{
	while ( context->unAckedSegs_.getSize() < context->peerOssMX_ ) {
		BufChain b = context->inpQ_->tryPop();
		if ( ! b )
			return;
		context->sendDAT( b );
	}
		
	context->usrIEH()->disable();
	context->changeState( &context->stateOPEN_OUTWIN_FULL );
}

void CRssi::OPEN::handleUsrOutputEvent(CRssi *context, IIntEventSource *src)
{
	// slots have become available in the output buffer
	context->usrOEH()->disable();

	// move from reassemble buffer to output queue
	drainReassembleBuffer( context );

	// Immediately ACK -- unless draining the reassemble buffer
	// again resulted in a full queue. The usrOEH was re-enabled
	// if this happened.
	if ( ! context->usrOEH()->isEnabled() ) {
		// immediately send ACK 
		context->sendACK();
	}
}

void CRssi::OPEN::drainReassembleBuffer(CRssi *context)
{
BufChain bc;

	while ( (bc = context->unOrderedSegs_.peek()) ) {
		Buf b = bc->getHead();

		RssiHeader hdr( b->getPayload() );

		if ( (hdr.getFlags() & RssiHeader::FLG_NUL) ) {
			context->unOrderedSegs_.pop();
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"RX: %s  NUL dumped ", context->getName());
hdr.dump(stderr, b->getSize() > hdr.getHSize());
fprintf(stderr," (state %s)\n", getName());
}
#endif
		} else {
			if ( ! context->outQ_->isFull() ) {
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"RX: %s  delivered  ", context->getName());
hdr.dump(stderr, b->getSize() > hdr.getHSize());
fprintf(stderr," (state %s)\n", getName());
}
#endif
				// we already hold a ref. in 'b'
				context->unOrderedSegs_.pop();
				// strip rssi header
				b->setPayload( b->getPayload() + hdr.getHSize() );
				b.reset();
				if ( ! context->outQ_->tryPush( bc ) ) {
					throw InternalError("RSSI output queue should not be full!");
				}
				context->stats_.numSegsGivenToUser_++;
			} else {
				context->usrOEH()->enable();
#ifdef RSSI_DEBUG
if (cpsw_rssi_debug > 1 )
{
fprintf(stderr,"RX: %s  NOT delivered  ", context->getName());
hdr.dump(stderr, b->getSize() > hdr.getHSize());
fprintf(stderr," (state %s)\n", getName());
}
#endif
				break;
			}
		}
	}

	context->lastSeqRecv_ = context->unOrderedSegs_.getLastInOrder();

}


void CRssi::OPEN_OUTWIN_FULL::handleRxEvent(CRssi *context, IIntEventSource *src)
{
	CRssi::OPEN::handleRxEvent(context, src);
	if ( context->unAckedSegs_.getSize() < context->peerOssMX_ ) {
		// can accept output again
		context->usrIEH()->enable();
		context->changeState( &context->stateOPEN );
	}
}


void CRssi::OPEN::processAckTimeout(CRssi *context)
{
BufChain bc = hasBufToSend(context);
	if ( bc )
		context->sendDAT( bc );
	else
		context->sendACK();
}

void CRssi::PREP_OPEN::advance(CRssi *context)
{
	context->usrIEH()->enable();
	context->changeState( &context->stateOPEN );
}

BufChain CRssi::NOTCLOSED::hasBufToSend(CRssi *context)
{
	return BufChain();
}

BufChain CRssi::OPEN::hasBufToSend(CRssi *context)
{
BufChain bc;
	if ( context->unAckedSegs_.getSize() < context->peerOssMX_ )
		bc = context->inpQ_->tryPop();
	return bc;
}
