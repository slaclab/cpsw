#include <cpsw_proto_mod_depack.h>
#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <stdio.h>
#include <errno.h>
#include <pthread.h>

#define DEPACK_DEBUG


static unsigned getNum(uint8_t *p, unsigned bit_offset, unsigned bit_size)
{
int i;
unsigned shift = bit_offset % 8;
unsigned rval;

	p += bit_offset/8;

	if ( bit_size == 0 )
		return 0;

	// protocol uses little-endian representation
	rval = 0;
	for ( i = (shift + bit_size + 7)/8 - 1; i>=0; i-- ) {
		rval = (rval << 8) + p[i];
	}

	rval >>= shift;

	rval &= (1<<bit_size)-1;

	return rval;
}

bool CAxisFrameHeader::parse(uint8_t *hdrBase, size_t hdrSize)
{
	if ( hdrSize  <  (VERSION_BIT_OFFSET + VERSION_BIT_SIZE + 7)/8 )
		return false; // cannot even read the version

	vers_ = getNum( hdrBase, VERSION_BIT_OFFSET, VERSION_BIT_SIZE );

	if ( vers_ != VERSION_0 )
		return false; // we don't know what size the header would be not its format

	if ( hdrSize < getSize() )
		return false;

	frameNo_ = getNum( hdrBase, FRAME_NO_BIT_OFFSET, FRAME_NO_BIT_SIZE );
	fragNo_  = getNum( hdrBase, FRAG_NO_BIT_OFFSET,  FRAG_NO_BIT_SIZE  );

	return true;
}


CProtoModDepack::CProtoModDepack(Key &k, CBufQueueBase::size_type oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, uint64_t timeoutUS)
	: CProtoMod(k, oqueueDepth),
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
	  timeout_( timeoutUS ),
	  frameWinSize_( 1<<ldFrameWinSize ),
	  fragWinSize_( 1<<ldFragWinSize ),
	  oldestFrame_( CFrame::NO_FRAME ),
	  frameWin_( frameWinSize_, CFrame(fragWinSize_) )
{
	if ( pthread_create( &tid_, 0, pthreadBody, this ) ) {
		throw IOError("unable to create thread ", errno);
	}
}

CProtoModDepack::CProtoModDepack(CProtoModDepack &orig, Key &k)
	: CProtoMod(orig, k),
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
	if ( pthread_create( &tid_, 0, pthreadBody, this ) ) {
		throw IOError("unable to create thread ", errno);
	}
}


CProtoModDepack::~CProtoModDepack()
{
void *ign;
	pthread_cancel( tid_ );
	pthread_join( tid_ , &ign );
}

void * CProtoModDepack::pthreadBody(void *arg)
{
CProtoModDepack *obj = static_cast<CProtoModDepack*>( arg );
	try {
		obj->threadBody();
	} catch ( CPSWError e ) {
		fprintf(stderr,"CPSW Error (CUdpHandlerThread): %s\n", e.getInfo().c_str());
		throw;
	}
	return 0;
}

void CProtoModDepack::threadBody()
{
	try {
		while ( 1 ) {
			CFrame *frame  = &frameWin_[ toFrameIdx( oldestFrame_ ) ];
#ifdef DEPACK_DEBUG
printf("depack: trying to pop\n");
#endif
			// wait for new datagram
			BufChain bufch = upstream_->pop( frame->running_ ? & frame->timeout_ : 0, IProtoPort::ABS_TIMEOUT );

			if ( ! bufch ) {
#ifdef DEPACK_DEBUG
{
CTimeout del;
	clock_gettime( CLOCK_REALTIME, &del.tv_ );
	if ( frame->running_ )
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
				Buf buf;

				while ( (buf = bufch->getHead()) ) {
					buf->unlink();
					processBuffer( buf );
					releaseFrames( true );
				}
			}
		}
	} catch ( IntrError e ) {
		// signal received; terminate...
	}
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

void CProtoModDepack::processBuffer(Buf b)
{
CAxisFrameHeader hdr;

	if ( ! hdr.parse( b->getPayload(), b->getSize() ) ) {
		badHeaderDrops_++;
		return;
	}

	// if there is a 'string' of frames that falls outside of the window
	// (which could happen during a 'resync') then we should readjust
	// the window...

	if ( hdr.getFrameNo() < oldestFrame_ ) {
		oldFrameDrops_++;
		frameSync( &hdr );
#ifdef DEPACK_DEBUG
printf("Dropping old frame %d (frag %d; oldest %d)\n", hdr.getFrameNo(), hdr.getFragNo(), oldestFrame_);
#endif
		return;
	}

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
	frame->fragWin_[fragIdx] = b;
	// Check for last frag
	if ( hdr.getTailEOF( b->getPayload() + b->getSize() - hdr.getTailSize() ) || ( CAxisFrameHeader::FRAG_MAX == hdr.getFragNo() ) ) {
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
		b->setPayload( b->getPayload() + hdr.getSize() );
	}

	if ( hdr.getFragNo() != frame->lastFrag_ ) {
		// skip tail on all but the last fragment
		b->setSize( b->getSize() - hdr.getTailSize() );
	}

	frame->updateChain();

}

bool CProtoModDepack::releaseOldestFrame(bool onlyComplete)
{
unsigned frameIdx      = oldestFrame_ & (frameWinSize_ - 1 );
CFrame  *frame         = &frameWin_[frameIdx];

	if ( ( onlyComplete && ! frame->isComplete() ) || ! frame->running_ )
		return false;

	BufChain completeFrame = frame->prod_;
	bool     isComplete    = frame->isComplete(); // frame invalid after release
#ifdef DEPACK_DEBUG
	bool     wasRunning    = frame->running_;
#endif

	frame->release( 0 );

	if ( isComplete ) {
		unsigned l = completeFrame->getLen();
		if ( ! pushDown( completeFrame, &TIMEOUT_INDEFINITE ) ) {
			oqueueFullDrops_++;
		} else {
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

	frame->timeout_ = getAbsTimeout( &timeout_ );

	frame->running_ = true;
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
