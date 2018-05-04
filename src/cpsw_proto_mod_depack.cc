 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_proto_mod_depack.h>
#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_thread.h>

#include <stdio.h>
#include <errno.h>

#include <cpsw_yaml.h>

//#define DEPACK_DEBUG

#define RSSI_PAYLOAD 1024


void CFrame::release(unsigned *ctr)
{
unsigned i;
	if ( ctr )
		(*ctr)++;
	prod_.reset();
	for ( i=0; i<fragWin_.size(); i++ )
		fragWin_[i].reset();
	nFrags_     = 0;
	frameID_    = NO_FRAME;
	lastFrag_   = NO_FRAG;
	oldestFrag_ = 0;
	isComplete_ = false;
	running_    = false;
}

void CFrame::updateChain()
{
unsigned idx = oldestFrag_ & (fragWin_.size() - 1);
	while ( fragWin_[idx] ) {
		Buf b;
		while ( (b = fragWin_[idx]->getHead()) ) {
			b->unlink();
			prod_->addAtTail( b );
		}
		nFrags_++;
		fragWin_[idx].reset();
		idx = (++oldestFrag_ & (fragWin_.size() - 1));
	}
	if ( nFrags_ == lastFrag_ + 1 || CAxisFrameHeader::FRAG_MAX == nFrags_ )
		isComplete_ = true;
}


CProtoModDepack::CProtoModDepack(Key &k, unsigned oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, CTimeout timeout, int threadPrio)
	: CProtoMod(k, oqueueDepth),
	  CRunnable("'Depacketizer' protocol module", threadPrio),
	  badHeaderDrops_(0),
	  oldFrameDrops_(0),
	  newFrameDrops_(0),
	  oldFragDrops_(0),
	  newFragDrops_(0),
	  duplicateFragDrops_(0),
	  duplicateLastSeen_(0),
	  noLastSeen_(0),
	  fragsAccepted_(0),
	  framesAccepted_(0),
	  oqueueFullDrops_(0),
	  evictedFrames_(0),
	  incompleteDrops_(0),
	  emptyDrops_(0),
	  timedOutFrames_(0),
	  pastLastDrops_(0),
	  cachedMTU_(0),
	  timeout_( timeout ),
	  frameWinSize_( 1<<ldFrameWinSize ),
	  fragWinSize_( 1<<ldFragWinSize ),
	  oldestFrame_( CFrame::NO_FRAME ),
	  frameWin_( frameWinSize_, CFrame(fragWinSize_) )
{
}

static unsigned ld(unsigned v)
{
unsigned rval = 0;
	if ( v & 0xffff0000 )
		rval += 16;
	if ( v & 0xff00ff00 )
		rval +=  8;
	if ( v & 0xf0f0f0f0 )
		rval +=  4;
	if ( v & 0xcccccccc )
		rval +=  2;
	if ( v & 0xaaaaaaaa )
		rval +=  1;
	return rval;	
}

void
CProtoModDepack::dumpYaml(YAML::Node &node) const
{
YAML::Node parms;
int prio = getPrio();

	writeNode(parms, YAML_KEY_outQueueDepth , getQueueDepth()    );
	writeNode(parms, YAML_KEY_ldFrameWinSize, ld( frameWinSize_ ));
	writeNode(parms, YAML_KEY_ldFragWinSize , ld( fragWinSize_ ) );
	if ( prio != IProtoStackBuilder::DFLT_THREAD_PRIORITY ) {
		writeNode(parms, YAML_KEY_threadPriority, prio);
	}
	writeNode(node, YAML_KEY_depack, parms);
}

void CProtoModDepack::modStartup()
{
	threadStart();
}

void CProtoModDepack::modShutdown()
{
	threadStop();
}


CProtoModDepack::CProtoModDepack(CProtoModDepack &orig, Key &k)
	: CProtoMod(orig, k),
	  CRunnable(orig),
	  badHeaderDrops_(0),
	  oldFrameDrops_(0),
	  newFrameDrops_(0),
	  oldFragDrops_(0),
	  newFragDrops_(0),
	  duplicateFragDrops_(0),
	  duplicateLastSeen_(0),
	  noLastSeen_(0),
	  fragsAccepted_(0),
	  framesAccepted_(0),
	  oqueueFullDrops_(0),
	  evictedFrames_(0),
	  incompleteDrops_(0),
	  emptyDrops_(0),
	  timedOutFrames_(0),
	  pastLastDrops_(0),
	  timeout_( orig.timeout_ ),
	  frameWinSize_( orig.frameWinSize_ ),
	  fragWinSize_( orig.fragWinSize_ ),
	  oldestFrame_( CFrame::NO_FRAME ),
	  frameWin_( frameWinSize_, CFrame(fragWinSize_) )
{
}


CProtoModDepack::~CProtoModDepack()
{
	threadStop();
}

bool
CProtoModDepack::fitsInMTU(unsigned sizeBytes)
{
	if ( sizeBytes <= SAFE_MTU )
		return true;
	if ( cachedMTU_ == 0 ) {
		cachedMTU_ = mustGetUpstreamDoor()->getMTU();
	}
	return sizeBytes <= cachedMTU_;
}

void * CProtoModDepack::threadBody()
{
	try {
		while ( 1 ) {
			CFrame *frame  = CFrame::NO_FRAME == oldestFrame_ ? NULL : &frameWin_[ toFrameIdx( oldestFrame_ ) ];
#ifdef DEPACK_DEBUG
printf("depack: trying to pop\n");
#endif

			// wait for new datagram
			BufChain bufch = upstream_->pop( frame && frame->running_ ? & frame->timeout_ : 0, IProtoPort::ABS_TIMEOUT );

			if ( ! bufch ) {
#ifdef DEPACK_DEBUG
{
CTimeout del;
	clock_gettime( CLOCK_REALTIME, &del.tv_ );
	if ( frame && frame->running_ )
		del -= frame->timeout_;
printf("Depack input timeout (late: %ld.%ld)\n", del.tv_.tv_sec, del.tv_.tv_nsec/1000);
}
#endif
				// timeout
				releaseOldestFrame( false );
				timedOutFrames_++;
				// maybe there are complete frames after the timed-out one
				releaseFrames( true );
			} else {
				processBuffer( bufch );
				releaseFrames( true );
			}
		}
	} catch ( IntrError e ) {
		// signal received; terminate...
	}
	return NULL;
}

void CProtoModDepack::frameSync(CAxisFrameHeader *hdr_p)
{
	// brute force for now...
	if ( abs( hdr_p->signExtendFrameNo( hdr_p->getFrameNo() - oldestFrame_ ) ) > frameWinSize_ ) {
		releaseFrames( false );
#ifdef DEPACK_DEBUG
printf("frameSync (frame %d, frag %d, oldest frame %d, winsz %d)\n",
		hdr_p->getFrameNo(),
		hdr_p->getFragNo(),
		oldestFrame_,
		frameWinSize_);
#endif
		oldestFrame_ = CAxisFrameHeader::moduloFrameSz( hdr_p->getFrameNo() + 1 );
	}
}

void CProtoModDepack::processBuffer(BufChain bc)
{
CAxisFrameHeader hdr;
Buf bh = bc->getHead();

	if ( ! hdr.parse( bh->getPayload(), bh->getSize() ) ) {
		badHeaderDrops_++;
		return;
	}

#ifdef DEPACK_DEBUG
printf("%s frame %d (frag %d; oldest %d)\n",
		hdr.getFrameNo() < oldestFrame_ && (CFrame::NO_FRAME != oldestFrame_ || 0 != hdr.getFragNo()) ? "Dropping" : "Accepting",
		hdr.getFrameNo(),
		hdr.getFragNo(),
		oldestFrame_
);
#endif

	// if there is a 'string' of frames that falls outside of the window
	// (which could happen during a 'resync') then we should readjust
	// the window...

	if ( hdr.getFrameNo() < oldestFrame_ ) {
		if ( CFrame::NO_FRAME == oldestFrame_ && 0 == hdr.getFragNo() ) {
			// special case - if this the first fragment then we accept 
			oldestFrame_ = hdr.getFrameNo();
		} else {
			oldFrameDrops_++;
			frameSync( &hdr );
			return;
		}
	}
	// at this point oldestFrame_ cannot be NO_FRAME

	FrameID relOff = CAxisFrameHeader::moduloFrameSz( hdr.getFrameNo() - oldestFrame_ );

	if ( relOff >= frameWinSize_ ) {
		// evict/drop oldest frame (which should be incomplete)
		if ( relOff == frameWinSize_ ) {
			evictedFrames_++;
			releaseOldestFrame( false );
#ifdef DEPACK_DEBUG
printf("Evict\n");
#endif
		} else {
			newFrameDrops_++;
			frameSync( &hdr );
#ifdef DEPACK_DEBUG
printf("Dropping new frame %d (frag %d; oldest %d, relOff %d)\n", hdr.getFrameNo(), hdr.getFragNo(), oldestFrame_, relOff);
#endif
			return;
		}
	}

	unsigned frameIdx = toFrameIdx( hdr.getFrameNo() );
	unsigned fragIdx  = toFragIdx( hdr.getFragNo() );

	CFrame *frame = &frameWin_[frameIdx];

	// oldestFrag_ of empty frame slots is 0 since they
	// expect to start with a new frame. Thus, the following
	// checks must succeed even if this is the first frag of
	// a new frame.
	if ( hdr.getFragNo() < frame->oldestFrag_ ) {
		// outside of window -- drop
		oldFragDrops_++;
#ifdef DEPACK_DEBUG
printf("Dropping old frag %d\n", hdr.getFragNo());
#endif
		return;
	}

	if ( hdr.getFragNo() >= frame->oldestFrag_ + fragWinSize_ ) {
		// outside of window -- drop
		newFragDrops_++;
#ifdef DEPACK_DEBUG
printf("Dropping new frag %d\n", hdr.getFragNo());
#endif
		return;
	}

	if ( CFrame::NO_FRAME == frame->frameID_ ) {
		// first frag of a frame -- may accept

		// Looks good
		frame->prod_             = IBufChain::create();
		frame->frameID_          = hdr.getFrameNo();

#ifdef DEPACK_DEBUG
printf("First frag (%d) of frame # %d\n", hdr.getFragNo(), frame->frameID_);
#endif

		// make sure older frames (which may not yet have received any fragment)
		// start their timeout
		for ( unsigned idx = toFrameIdx( oldestFrame_ ); idx != frameIdx; idx = toFrameIdx( idx + 1 ) )
			startTimeout( & frameWin_[idx] );
		startTimeout( frame );
	} else {
		if ( frame->frameID_ != hdr.getFrameNo() ) {
			// since frameID >= oldestFrame && frameID < oldestFrame + frameWinSize
			// this should never happen.
			fprintf(stderr,"working on frame %d -- but %d found in its slot!\n", hdr.getFrameNo(), frame->frameID_);
			throw InternalError("Frame ID window inconsistency!");
		}
		if ( frame->fragWin_[fragIdx] ) {
			duplicateFragDrops_++;
			return;
		}

		if ( CFrame::NO_FRAG != frame->lastFrag_ && hdr.getFragNo() > frame->lastFrag_ ) {
			pastLastDrops_++;
			return;
		}
#ifdef DEPACK_DEBUG
printf("Frag %d of frame # %d\n", hdr.getFragNo(), frame->frameID_);
#endif
	}

	// Looks good - a new frag
	frame->fragWin_[fragIdx] = bc;
	// Check for last frag

	Buf bt = bc->getTail();

	if ( hdr.parseTailEOF( bt->getPayload() + bt->getSize() - hdr.getTailSize() ) || ( CAxisFrameHeader::FRAG_MAX == hdr.getFragNo() ) ) {
		if ( CFrame::NO_FRAG != frame->lastFrag_ ) {
			duplicateLastSeen_++;
#ifdef DEPACK_DEBUG
printf("DuplicateLast\n");
#endif
		} else {
			if ( CAxisFrameHeader::FRAG_MAX == (frame->lastFrag_ = hdr.getFragNo()) ) {
				noLastSeen_++;
			}
#ifdef DEPACK_DEBUG
printf("Last frag %d\n", frame->lastFrag_);
#endif
		}
	}

	if ( hdr.getFragNo() != 0 ) {
		// skip header on all but the first fragments
		bh->setPayload( bh->getPayload() + hdr.getSize() );
	}

	if ( hdr.getFragNo() != frame->lastFrag_ ) {
		// skip tail on all but the last fragment
		bt->setSize( bt->getSize() - hdr.getTailSize() );
	}

	frame->updateChain();

}

bool CProtoModDepack::releaseOldestFrame(bool onlyComplete)
{

	if ( CFrame::NO_FRAME == oldestFrame_ )
		return false;

	unsigned frameIdx      = oldestFrame_ & (frameWinSize_ - 1 );
	CFrame  *frame         = &frameWin_[frameIdx];

#ifdef DEPACK_DEBUG
	printf("releaseOldestFrame onlyComplete %d, isComplete %d, running %d\n", onlyComplete, frame->isComplete(), frame->running_);
#endif

	if ( ( onlyComplete && ! frame->isComplete() ) || ! frame->running_ )
		return false;

	BufChain completeFrame = frame->prod_;
	bool     isComplete    = frame->isComplete(); // frame invalid after release
#ifdef DEPACK_DEBUG
	bool     wasRunning    = frame->running_;
#endif

	frame->release( 0 );

	if ( isComplete ) {
#ifdef DEPACK_DEBUG
	printf("PUSHDOWN FRAME %d", frame->frameID_);
#endif
		unsigned l = completeFrame->getLen();
		if ( ! pushDown( completeFrame, &TIMEOUT_INDEFINITE ) ) {
#ifdef DEPACK_DEBUG
	printf(" => DROPPED\n");
#endif
			oqueueFullDrops_++;
		} else {
#ifdef DEPACK_DEBUG
	printf("\n");
#endif
			fragsAccepted_  += l;
			framesAccepted_ += 1;
		}
	} else {
		if ( completeFrame )
			incompleteDrops_++;
		else
			emptyDrops_++;
	}

	oldestFrame_ = CAxisFrameHeader::moduloFrameSz( oldestFrame_ + 1 );

#ifdef DEPACK_DEBUG
	printf("Updated oldest to %d (is complete %d, running %d)\n", oldestFrame_, isComplete, wasRunning);
#endif

	return true;
}

void
CProtoModDepack::appendTailByte(BufChain bc, bool isEof)
{
size_t new_sz;
Buf    b = bc->getTail();

	// append tailbyte

	new_sz = b->getSize() + CAxisFrameHeader::getTailSize();
	if ( fitsInMTU( new_sz ) ) {
		b->setSize( new_sz );
	} else {
		b = bc->createAtTail( IBuf::CAPA_ETH_HDR );
		b->setSize( CAxisFrameHeader::getTailSize() );
	}

	CAxisFrameHeader::iniTail      ( b->getPayload() + b->getSize() - CAxisFrameHeader::getTailSize() );
	CAxisFrameHeader::insertTailEOF( b->getPayload() + b->getSize() - CAxisFrameHeader::getTailSize(), isEof );
}

BufChain CProtoModDepack::processOutput(BufChain *bcp)
{
#define SAFETY 16 // in case there are other protocols
BufChain bc = *bcp;
BufChain res;

Buf      b;
FrameID  frameNo;
FragID   fragNo;
size_t   sz, bsz;

	// Insert new frame and frag numbers into the header supplied by upper layer

	b = bc->getHead();

CAxisFrameHeader hdr( b->getPayload(), b->getSize() );

	fragNo = hdr.getFragNo();

	if ( 0 == fragNo ) {
		frameNo = frameIdGen_.newFrameID();
		hdr.setFrameNo( frameNo );
		hdr.setSOF(true);
	}

	hdr.insert( b->getPayload(), hdr.getSize() );



#ifdef DEPACK_DEBUG
	if ( fragNo >= 0 ) {
		printf("Depack: fragmented output frame %d, frag %d, initial bc size %ld, len %d\n", hdr.getFrameNo(), fragNo, (long)bc->getSize(), bc->getLen());
	}
#endif

	// ugly hack - limit to ethernet RSSI payload
	if ( fitsInMTU( bc->getSize() ) ) {
#ifdef DEPACK_DEBUG
		if ( fragNo > 0 ) {
			printf("Depack: fragmented output (tail); frag %d\n", fragNo);
		}
#endif
		appendTailByte( bc, true );
		res.swap( *bcp );
	} else {
		sz  = 0;
		b   = bc->getHead();
		bsz = b->getSize();
		res = IBufChain::create();
#ifdef DEPACK_DEBUG
		printf("Depack: next fragment size %ld\n", (long)bsz);
#endif
		while ( fitsInMTU( sz + bsz ) ) {
			b->unlink();
			res->addAtTail( b );
			sz += bsz;
			b   = bc->getHead();
			bsz = b->getSize();
#ifdef DEPACK_DEBUG
		printf("Depack: next fragment size %ld\n", (long)bsz);
#endif
		}

		if ( sz == 0 ) {
			// We could copy into a new chain of smaller buffers
			// but we'd rather the caller do that to avoid copying
			throw InternalError("Depacketizer: Unable to fragment");
		}

		// make a new header
		CAxisFrameHeader hdr1( hdr );
		hdr1.setFragNo( fragNo + 1 );
		hdr1.setSOF( false );

		if ( ! b->adjPayload( -hdr1.getSize() ) ) {
#ifdef DEPACK_DEBUG
			printf("Depack: added new header for next fragment\n");
#endif
			b = IBuf::getBuf( IBuf::CAPA_ETH_HDR );
			bc->addAtHead( b );
		}
		hdr1.insert( b->getPayload(), hdr1.getSize() );

		appendTailByte( res, false );

#ifdef DEPACK_DEBUG
		if ( fragNo > 0 ) {
			printf("Depack: fragmented output frame %d, frag %d, final bc size %ld, len %d\n", hdr1.getFrameNo(), hdr1.getFragNo(), (long)bc->getSize(), bc->getLen());
			printf("Depack: fragmented output result size %ld, len %d\n", (long)res->getSize(), res->getLen());
		}
#endif
	}

	return res;
}

void CProtoModDepack::releaseFrames(bool onlyComplete)
{
	while ( releaseOldestFrame( onlyComplete ) )
		/* nothing else to do */;
}

void CProtoModDepack::startTimeout(CFrame *frame)
{
struct timespec now;

	if ( frame->running_ )
		return;

	if ( clock_gettime( CLOCK_REALTIME, &now ) ) 
		throw InternalError("clock_gettime failed");

	frame->timeout_ = upstream_->getAbsTimeoutPop( &timeout_ );

	frame->running_ = true;
}

int CProtoModDepack::iMatch(ProtoPortMatchParams *cmp)
{
	cmp->haveDepack_.handledBy_ = getProtoMod();
	if ( cmp->haveDepack_.doMatch_ && cmp->depackVersion_ == CAxisFrameHeader::VERSION ) {
		cmp->haveDepack_.matchedBy_ = getSelfAs<ProtoModDepack>();
		return 1;
	}
	return 0;
}

unsigned
CProtoModDepack::getMTU()
{
	return mustGetUpstreamDoor()->getMTU() - CAxisFrameHeader::HEADER_SIZE - CAxisFrameHeader::TAIL_SIZE;
}


bool
CProtoModDepack::push(BufChain bc, const CTimeout *to, bool abs_timeout)
{
bool rval = false;
	try {
		rval = CProtoMod::push( bc, to, abs_timeout );
	} catch (InternalError &e) {
		// if RSSI complains then try resetting the cache
		if ( cachedMTU_ == 0 ) {
			throw;
		}
	}
	if ( ! rval ) {
		cachedMTU_ = 0;
	}
	return rval;
}

bool
CProtoModDepack::tryPush(BufChain bc)
{
bool rval = false;
	try {
		rval = CProtoMod::tryPush( bc );
	} catch (InternalError &e) {
		// if RSSI complains then try resetting the cache
		if ( cachedMTU_ == 0 ) {
			throw;
		}
	}
	if ( ! rval ) {
		cachedMTU_ = 0;
	}
	return rval;
}


void CProtoModDepack::dumpInfo(FILE *f)
{
	if ( ! f )
		f = stdout;
	fprintf(f,"CProtoModDepack:\n");
	fprintf(f,"  Frame Window Size: %4d, Frag Window Size: %4d\n", frameWinSize_, fragWinSize_);
	fprintf(f,"  Timeout          : %4ld.%09lds\n", timeout_.tv_.tv_sec, timeout_.tv_.tv_nsec);
	fprintf(f,"  **Good Fragments Accepted     **: %8d\n", fragsAccepted_);
	fprintf(f,"  **Good Frames Accepted        **: %8d\n", framesAccepted_);
	fprintf(f,"  Frames dropped due to bad header: %8d\n", badHeaderDrops_);
	fprintf(f,"  Frames dropped below Frame Win  : %8d\n", oldFrameDrops_);
	fprintf(f,"  Frames dropped beyond Frame Win : %8d\n", newFrameDrops_);
	fprintf(f,"  Frags  dropped below Frag Window: %8d\n", oldFragDrops_);
	fprintf(f,"  Frags  dropped beyond Frag Win  : %8d\n", newFragDrops_);
	fprintf(f,"  Duplicates of Fragments dropped : %8d\n", duplicateFragDrops_);
	fprintf(f,"  Duplicate EOF seen              : %8d\n", duplicateLastSeen_);
	fprintf(f,"  No EOF seen                     : %8d\n", noLastSeen_);
	fprintf(f,"  Frames dropped due outQueue full: %8d\n", oqueueFullDrops_);
	fprintf(f,"  Frames dropped due to Eviction  : %8d\n", evictedFrames_);
	fprintf(f,"  Incomplete Frames dropped (sync): %8d\n", incompleteDrops_);
	fprintf(f,"  Empty fragments dropped         : %8d\n", emptyDrops_);
	fprintf(f,"  Incomplete Frames with Timeout  : %8d\n", timedOutFrames_);
	fprintf(f,"  Frames past EOF dropped         : %8d\n", pastLastDrops_);
}
