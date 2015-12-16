#include <cpsw_proto_mod_depack.h>
#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <errno.h>
#include <pthread.h>


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


CProtoModDepack::CProtoModDepack(CProtoModKey k, CBufQueueBase::size_type oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned long timeoutUS)
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
	  frameWinSize_( 1<<ldFrameWinSize ),
	  fragWinSize_( 1<<ldFragWinSize ),
	  oldestFrame_( CFrame::NO_FRAME ),
	  frameWin_( frameWinSize_, CFrame(fragWinSize_) )
{
	timeout_.tv_sec  = timeoutUS / 1000000UL;
	timeout_.tv_nsec = (timeoutUS % 1000000UL) * 1000;

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
	obj->threadBody();
	return 0;
}

void CProtoModDepack::threadBody()
{
	try {
		while ( 1 ) {
			CFrame *frame  = &frameWin_[ toFrameIdx( oldestFrame_ ) ];
			// wait for new datagram
			BufChain bufch = upstream_->pop( frame->running_ ? & frame->timeout_ : 0 );

			if ( ! bufch ) {
				// timeout
				releaseOldestFrame( false );
				timedOutFrames_++;
				// maybe there are complete frames after the timed-out one
				releaseFrames( true );
			} else {
				Buf buf = bufch->getHead();
				buf->unlink();
				processBuffer( buf );
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
		releaseFrames( true );
		oldestFrame_ = hdr_p->getFrameNo() + 1;
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
		return;
	}

	if ( hdr.getFrameNo() >= oldestFrame_ + frameWinSize_ ) {
		// evict/drop oldest frame (which should be incomplete)
		if ( hdr.getFrameNo() == oldestFrame_ + frameWinSize_ ) {
			evictedFrames_++;
			releaseOldestFrame( false );
		} else {
			newFrameDrops_++;
			frameSync( &hdr );
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
		return;
	}

	if ( hdr.getFragNo() >= frame->oldestFrag_ + fragWinSize_ ) {
		// outside of window -- drop
		newFragDrops_++;
		return;
	}

	if ( CFrame::NO_FRAME == frame->frameID_ ) {
		// first frag of a frame -- may accept

		// Looks good
		frame->prod_             = IBufChain::create();
		frame->frameID_          = hdr.getFrameNo();

		// make sure older frames (which may not yet have received any fragment)
		// start their timeout
		for ( unsigned idx = toFrameIdx( oldestFrame_ ); idx != frameIdx; idx = toFrameIdx( idx + 1 ) )
			startTimeout( & frameWin_[idx] );
		startTimeout( frame );
	} else {
		if ( frame->frameID_ != hdr.getFrameNo() ) {
			// since frameID >= oldestFrame && frameID < oldestFrame + frameWinSize
			// this should never happen.
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
	}

	// Looks good - a new frag
	frame->fragWin_[fragIdx] = b;
	// Check for last frag
	if ( hdr.getTailEOF( b->getPayload() + b->getSize() - hdr.getTailSize() ) || ( CAxisFrameHeader::FRAG_MAX == hdr.getFragNo() ) ) {
		if ( CFrame::NO_FRAG != frame->lastFrag_ ) {
			duplicateLastSeen_++;
		} else {
			if ( CAxisFrameHeader::FRAG_MAX == (frame->lastFrag_ = hdr.getFragNo()) ) {
				noLastSeen_++;
			}
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


	if ( frame->isComplete() )
		releaseFrames( true );
}

bool CProtoModDepack::releaseOldestFrame(bool onlyComplete)
{
unsigned frameIdx      = oldestFrame_ & (frameWinSize_ - 1 );
CFrame  *frame         = &frameWin_[frameIdx];
unsigned l;

	if ( ( onlyComplete && ! frame->isComplete() ) || ! frame->running_ )
		return false;

	BufChain completeFrame = frame->prod_;

	frame->release( 0 );

	if ( frame->isComplete() ) {
		if ( ! outputQueue_.push( &completeFrame ) ) {
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

	oldestFrame_++;

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

	frame->timeout_.tv_nsec = now.tv_nsec + timeout_.tv_nsec;
	frame->timeout_.tv_sec  = now.tv_sec +  timeout_.tv_sec;

	if ( frame->timeout_.tv_nsec >= 1000000000L ) {
		frame->timeout_.tv_nsec -= 1000000000L;
		frame->timeout_.tv_sec  += 1;
	}

	frame->running_ = true;
}


