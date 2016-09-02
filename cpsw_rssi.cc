 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_rssi.h>

#include <vector>

using std::vector;

#ifdef RSSI_DEBUG
int rssi_debug = RSSI_DEBUG;
#endif

void CRssi::attach(IEventSource *upstreamReadEventSource)
{
#ifdef RSSI_DEBUG
if ( rssi_debug > 3 )
{
fprintf(stderr,"%s: attach\n", ""); 
}
#endif
	eventSet_->add( upstreamReadEventSource, recvEH() );
#ifdef RSSI_DEBUG
if ( rssi_debug > 3 )
{
fprintf(stderr,"%s: upstream port queue created\n", "");
}
#endif
}
	
CRssi::CRssi(bool isServer)
: CRunnable("RSSI Thread"),
  IRexTimer( &timers_ ),
  IAckTimer( &timers_ ),
  INulTimer( &timers_ ),
  isServer_( isServer ),

  name_( isServer ? "S" : "C" ),
  outQ_    ( IBufQueue::create( OQ_DEPTH ) ),
  inpQ_    ( IBufQueue::create( IQ_DEPTH ) ),

  state_( &stateCLOSED ),
  timerUnits_( UNIT_US ),
  unAckedSegs_( LD_MAX_UNACKED_SEGS ),
  unOrderedSegs_( LD_MAX_UNACKED_SEGS, MAX_UNACKED_SEGS )
{
	conID_ = (uint32_t)time(NULL);
	::memset( &stats_, 0, sizeof(stats_) );
	closedReopenDelay_.tv_nsec = 0;
	closedReopenDelay_.tv_sec  = 0;

	eventSet_ = IEventSet::create();
	eventSet_->add( inpQ_->getReadEventSource() , usrIEH() );
	eventSet_->add( outQ_->getWriteEventSource(), usrOEH() );
}

void CRssi::processAckNumber(uint8_t flags, SeqNo ackNo)
{
int acked;
	if ( (flags & RssiHeader::FLG_ACK) ) {
		if ( (acked = unAckedSegs_.ack( ackNo )) > 0 ) {
			numRex_ = 0;
			stats_.numSegsAckedByPeer_ += acked;
		}
		if ( unAckedSegs_.getSize() == 0 ) {
#ifdef RSSI_DEBUG
if ( rssi_debug > 3 )
{
fprintf(stderr,"%s: got all ACKS (oldest %d)\n", getName(), unAckedSegs_.getOldest());
}
#endif
			rexTimer()->cancel();
			numRex_ = 0;
		} else {
#ifdef RSSI_DEBUG
if ( rssi_debug > 3 )
{
fprintf(stderr,"%s: outstanding ACKS %d (oldest %d)\n", getName(), unAckedSegs_.getSize(), unAckedSegs_.getOldest());
}
#endif
		}
	}
}

void CRssi::handleRxEvent(IIntEventSource *src)
{
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: RX Event (state %s)\n", getName(), state_->getName());
}
#endif
	state_->handleRxEvent(this, src);
}

void CRssi::handleUsrInputEvent(IIntEventSource *src)
{
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: USR Input Event (state %s)\n", getName(), state_->getName());
}
#endif
	state_->handleUsrInputEvent(this, src);
}

void CRssi::handleUsrOutputEvent(IIntEventSource *src)
{
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: USR Output Event (state %s)\n", getName(), state_->getName());
}
#endif
	state_->handleUsrOutputEvent(this, src);
}

void CRssi::close()
{
	outQ_->shutdown();
	outQ_->startup();
	inpQ_->shutdown();
	inpQ_->startup();
	usrIEH()->disable();
	usrOEH()->disable();
	ackTimer()->cancel();
	nulTimer()->cancel();
	rexTimer()->cancel();

	units_ = UNIT_US;

	// initial default values
	rexTO_ = CTimeout(  RETRANSMIT_TIMEO * units_ );
	cakTO_ = CTimeout(  CUMLTD_ACK_TIMEO * units_ );
	nulTO_ = CTimeout( (NUL_SEGMEN_TIMEO * units_ ) / (isServer_ ? 1 : 3) );
	rexMX_ = MAX_RETRANSMIT_N;
	cakMX_ = MAX_CUMLTD_ACK_N;

	unAckedSegs_.purge();
	unOrderedSegs_.purge();

	numRex_         = 0;
	numCak_         = 0;

	peerBSY_        = false;

	// we don't know the peer's setting yet (it's in the SYN packet)
	// -> don't verify their checksum but always add ours
	verifyChecksum_ = false;
	addChecksum_    = true;

	peerOssMX_      = MAX_UNACKED_SEGS;
	peerSgsMX_      = MAX_SEGMENT_SIZE;

	conID_++;
}

void CRssi::sendBuf(BufChain bc, bool retrans)
{
	if ( bc->getSize() > peerSgsMX_ ) {
		throw InternalError("RSSI segment size too large for peer");
	}

	Buf b = bc->getHead();

	RssiHeader hdr( b->getPayload() );

	if ( outQ_->isFull() ) {
		hdr.setFlags( hdr.getFlags() | RssiHeader::FLG_BSY );
	}

	hdr.setAckNo( lastSeqRecv_ );

#ifdef RSSI_DEBUG
if ( rssi_debug > 0 )
{
fprintf(stderr,"tX: %s -- ", getName());
hdr.dump( stderr, b->getSize() > hdr.getHSize() ? 1 : 0 );
fprintf(stderr," (state %s) %s", state_->getName(), retrans ? "REX" : "");
}
#endif

	numCak_ = 0;
	ackTimer()->cancel();

	if ( addChecksum_ )
		hdr.writeChksum();

	if ( ! tryPushUpstream( bc ) ) {
#ifdef RSSI_DEBUG
if ( rssi_debug > 0 )
{
fprintf(stderr," => No space in upstream queue!");
}
#endif
		stats_.outgoingDropped_++;
	}
#ifdef RSSI_DEBUG
if ( rssi_debug > 0 )
{
fprintf(stderr,"\n");
}
#endif
}

void CRssi::armRexAndNulTimer()
{
	CTimeout now;

	if ( ! peerBSY_ || ! isServer_ ) {
		eventSet_->getAbsTime( &now );

		if ( ! peerBSY_ ) {
			rexTimer()->arm_abs( now + rexTO_ );
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: REX timer armed (state %s)\n", getName(), state_->getName());
}
#endif
		}

		if ( ! isServer_ ) {
			nulTimer()->arm_abs( now + nulTO_ );
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: NUL timer armed (state %s)\n", getName(), state_->getName());
}
#endif
		}
	}
}

void CRssi::sendBufAndKeepForRetransmission(BufChain b)
{
	sendBuf( b, false );
	unAckedSegs_.push( b );
	armRexAndNulTimer();

	unAckedSegs_.dump();
}

void CRssi::sendDAT(BufChain bc)
{
Buf b = bc->getHead();
	// assume output window has already been checked
	b->setPayload( b->getPayload() - RssiHeader::getHSize( RssiHeader::getSupportedVersion() ) );
	RssiHeader hdr( b->getPayload(), b->getSize(), false, RssiHeader::SET );
	hdr.setFlags( RssiHeader::FLG_ACK );
	hdr.setSeqNo( ++lastSeqSent_ );
	sendBufAndKeepForRetransmission( bc );
}

void CRssi::sendACK()
{
BufChain   bc = IBufChain::create();
Buf        b  = bc->createAtHead( IBuf::CAPA_ETH_HDR );
RssiHeader hdr( b->getPayload(), b->getCapacity(), false, RssiHeader::SET );

	hdr.setFlags( RssiHeader::FLG_ACK );
	hdr.setSeqNo( lastSeqSent_ + 1 );

	b->setSize( hdr.getHSize() );

	// ACK is never retransmitted
	sendBuf( bc, false );

#ifdef RSSI_DEBUG
if ( rssi_debug > 1 )
{
fprintf(stderr,"%s: sent ACKONLY %d\n", getName(), lastSeqSent_ + 1);
}
#endif
}

void CRssi::sendRST()
{
BufChain   bc = IBufChain::create();
Buf        b  = bc->createAtHead( IBuf::CAPA_ETH_HDR );
RssiHeader hdr( b->getPayload(), b->getCapacity(), false, RssiHeader::SET );

	hdr.setFlags( RssiHeader::FLG_ACK | RssiHeader::FLG_RST );
	hdr.setSeqNo( ++lastSeqSent_ );

	b->setSize( hdr.getHSize() );

	// RST is never retransmitted
	sendBuf( bc, false );

#ifdef RSSI_DEBUG
if ( rssi_debug > 1 )
{
fprintf(stderr,"%s: sent RESET %d\n", getName(), lastSeqSent_);
}
#endif
}


bool CRssi::sendNUL()
{
	// can only send if we have slots in our outgoing window
	if ( unAckedSegs_.getSize() >= peerOssMX_ ) {
		// 
		stats_.skippedNULs_++;
		nulTimer()->arm_rel( nulTO_ );
		// send an ACK at least
		sendACK();
		return false;
	} else {
		BufChain   bc = IBufChain::create();
		Buf        b  = bc->createAtHead( IBuf::CAPA_ETH_HDR );
		RssiHeader hdr( b->getPayload(), b->getCapacity(), false, RssiHeader::SET );

		hdr.setFlags( RssiHeader::FLG_ACK | RssiHeader::FLG_NUL );
		hdr.setSeqNo( ++lastSeqSent_ );

		b->setSize( hdr.getHSize() );

		// ACK is never retransmitted
		sendBufAndKeepForRetransmission( bc );

#ifdef RSSI_DEBUG
if ( rssi_debug > 1 )
{
fprintf(stderr,"%s: sent NUL %d\n", getName(), lastSeqSent_);
}
#endif
		return true;
	}
}


void CRssi::sendSYN(bool doAck)
{
BufChain      bc    = IBufChain::create();
Buf           b     = bc->createAtHead( IBuf::CAPA_ETH_HDR );
RssiSynHeader synHdr( b->getPayload(), b->getCapacity(), false, RssiHeader::SET );
uint8_t       flags = RssiHeader::FLG_SYN;

	if ( doAck ) {
		flags |= RssiHeader::FLG_ACK;
	}

	synHdr.setFlags( flags );
	synHdr.setSeqNo( ( lastSeqSent_ = (SeqNo)(time(NULL) ^ (isServer_ ? -1 : 0) ) ) );

	flags = RssiSynHeader::XFL_ONE;
	if ( addChecksum_ )
		flags |= RssiSynHeader::XFL_CHK;

	synHdr.setXflgs( flags );
	synHdr.setVersn( RssiSynHeader::RSSI_VERSION_1 );
	synHdr.setOssMX( MAX_UNACKED_SEGS );
	synHdr.setSgsMX( MAX_SEGMENT_SIZE );
	synHdr.setRexTO( RETRANSMIT_TIMEO );
	synHdr.setCakTO( CUMLTD_ACK_TIMEO );
	synHdr.setNulTO( NUL_SEGMEN_TIMEO );
	synHdr.setRexMX( MAX_RETRANSMIT_N );
	synHdr.setCakMX( MAX_CUMLTD_ACK_N );
	synHdr.setOsaMX( 0 );
	synHdr.setUnits( UNIT_US_EXP );
	synHdr.setConID( conID_ );

	unAckedSegs_.seed( lastSeqSent_ );

	b->setSize( synHdr.getHSize() );

	sendBufAndKeepForRetransmission( bc );

#ifdef RSSI_DEBUG
if ( rssi_debug > 1 )
{
fprintf(stderr,"%s: SYN sent\n", getName());
}
#endif
}

void CRssi::processRetransmissionTimeout()
{
	stats_.rexTimeouts_++;
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: RexTimer expired\n", getName());
}
#endif
	state_->processRetransmissionTimeout( this );
}

void CRssi::processAckTimeout()
{
	stats_.ackTimeouts_++;
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: AckTimer expired\n", getName());
}
#endif
	state_->processAckTimeout( this );
}

void CRssi::processNulTimeout()
{
	stats_.nulTimeouts_++;
#ifdef RSSI_DEBUG
if ( rssi_debug > 2 )
{
fprintf(stderr,"%s: NulTimer expired\n", getName());
}
#endif
	state_->processNulTimeout( this );
}

void * CRssi::threadBody()
{

	while ( 1 ) {
		state_->advance( this );
	}

	return NULL;
}

void CRssi::changeState(STATE *newState)
{
int newConnState = newState->getConnectionState( this );
int oldConnState = state_->getConnectionState( this );

#ifdef RSSI_DEBUG
if ( rssi_debug > 1 )
{
	fprintf(stderr,"%s: Scheduling state change %s ==> %s\n", getName(), state_->getName(), newState->getName());
}
#endif
	state_ = newState;
	if ( newConnState && newConnState != oldConnState ) {
		CConnectionOpenEventSource::connectionStateChanged   ( newConnState );
		CConnectionNotOpenEventSource::connectionStateChanged( newConnState );
		CConnectionClosedEventSource::connectionStateChanged ( newConnState );
	}
}

CConnectionStateEventSource::CConnectionStateEventSource()
: connState_(CONN_STATE_CLOSED)
{
}

void CConnectionStateEventSource::connectionStateChanged(int newConnState)
{
	connState_.store( newConnState, memory_order_release );
	notify();
}

CConnectionOpenEventSource::CConnectionOpenEventSource()
{
	setEventVal(1);
}

CConnectionNotOpenEventSource::CConnectionNotOpenEventSource()
{
	setEventVal(1);
}

CConnectionClosedEventSource::CConnectionClosedEventSource()
{
	setEventVal(1);
}


bool CConnectionOpenEventSource::checkForEvent()
{
	return connState_.load( memory_order_acquire ) == CONN_STATE_OPEN;
}

bool CConnectionNotOpenEventSource::checkForEvent()
{
	return connState_.load( memory_order_acquire ) != CONN_STATE_OPEN;
}

bool CConnectionClosedEventSource::checkForEvent()
{
	return connState_.load( memory_order_acquire ) == CONN_STATE_CLOSED;
}

CRssi::~CRssi()
{
/* derived class has to call this
	threadStop();
 */
}

void CRssi::dumpStats(FILE *f)
{
	fprintf(f ,"RSSI (%s) statistics (%s mode):\n", getName(), isServer_ ? "Server" : "Client");
	fprintf(f ,"  # outgoing segments dropped: %12u\n", stats_.outgoingDropped_   );
	fprintf(f ,"  # retransmitted segments   : %12u\n", stats_.rexSegments_       );
	fprintf(f ,"  # retransmission timeouts  : %12u\n", stats_.rexTimeouts_       );
	fprintf(f ,"  # retrans. due to BSY deass: %12u\n", stats_.busyDeassertRex_   );
	fprintf(f ,"  # cumulative ACK timeouts  : %12u\n", stats_.ackTimeouts_       );
	fprintf(f ,"  # NUL periods expired      : %12u\n", stats_.nulTimeouts_       );
	fprintf(f ,"  # Headers with bad checksum: %12u\n", stats_.badChecksum_       );
	fprintf(f ,"  # Invalid SYN headers      : %12u\n", stats_.badSynDropped_     );
	fprintf(f ,"  # Invalid headers          : %12u\n", stats_.badHdrDropped_     );
	fprintf(f ,"  # RXsegs out of WIN        : %12u\n", stats_.rejectedSegs_      );
	fprintf(f ,"  # NUL replaced by ACK      : %12u\n", stats_.skippedNULs_       );
	fprintf(f ,"  # segments ACKed by peer   : %12u\n", stats_.numSegsAckedByPeer_);
	fprintf(f ,"  # segments delivered to usr: %12u\n", stats_.numSegsGivenToUser_);
	fprintf(f ,"  # segs with BSY asserted   : %12u\n", stats_.busyFlagsCounted_  );
}

void CRssi::RingBuf::dump()
{
iterator it(this);
BufChain bc;
	while ( (bc = *it) ) {
		Buf b = bc->getHead();
		RssiHeader hdr(b->getPayload());
#ifdef RSSI_DEBUG
		if ( rssi_debug > 3 ) {
			fprintf(stderr,"   >>>[%d - %p] ", it.idx_, b.get());
			hdr.dump(stderr, b->getSize() > hdr.getHSize());
			fprintf(stderr,"\n");
		}
#endif
		++it;
	}
}

bool CRssi::threadStop()
{
bool rval;

	rval = CRunnable::threadStop();
	state_->shutdown( this );
	close();
	state_ = &stateCLOSED;

	return rval;
}
