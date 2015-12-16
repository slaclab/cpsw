#ifndef CPSW_PROTO_MOD_DEPACK_H
#define CPSW_PROTO_MOD_DEPACK_H

#include <boost/intrusive/list.hpp>
#include <vector>

#include <cpsw_api_user.h>
#include <cpsw_proto_mod.h>

#include <pthread.h>

using std::vector;

typedef unsigned FrameID;
typedef unsigned FragID;
typedef unsigned ProtoVersion;

class IProtoModDepack : public virtual IProtoMod {
};

class CAxisFrameHeader {
public:
	FrameID      frameNo_;
	FragID       fragNo_;
	ProtoVersion vers_;

	static const unsigned VERSION_BIT_OFFSET  =  0;
	static const unsigned VERSION_BIT_SIZE    =  4;
	static const unsigned FRAME_NO_BIT_OFFSET =  4;
	static const unsigned FRAME_NO_BIT_SIZE   = 12;
	static const unsigned FRAG_NO_BIT_OFFSET  = 16; 
	static const unsigned FRAG_NO_BIT_SIZE    = 24; 
	static const unsigned FRAG_MAX            = (1<<FRAG_NO_BIT_SIZE) - 1;

	static const unsigned FRAG_LAST_BIT       =  7;

	static const unsigned VERSION_0           =  0;

	static const unsigned V0_HEADER_SIZE      =  8;

	static const unsigned V0_TAIL_SIZE        =  1;

public:
	bool parse(uint8_t *hdrBase, size_t hdrSize);

	FrameID      getFrameNo() { return frameNo_; }
	FragID       getFragNo()  { return fragNo_;  }
	ProtoVersion getVersion() { return vers_;    }

	static int   signExtendFrameNo(FrameID x)
	{
		if ( x & (1<<(FRAME_NO_BIT_SIZE-1)) )
			 x |= ~((1<<FRAME_NO_BIT_SIZE)-1);
		return x;
	}

	size_t       getSize()     { return V0_HEADER_SIZE; }

	size_t       getTailSize() { return V0_TAIL_SIZE;   }

	static bool getTailEOF(uint8_t *tailbuf) { return (*tailbuf) & (1<<FRAG_LAST_BIT); }
};

class CFrame {
public:
	static const FrameID NO_FRAME = (FrameID)-1;
	static const FragID  NO_FRAG  = (FragID)-2; // so that NO_FRAG + 1 is not a valid ID
protected:
	BufChain         prod_;
	FrameID          frameID_;
	FragID           oldestFrag_;
	FragID           lastFrag_;
	CTimeout         timeout_;
	vector<Buf>      fragWin_;	
	bool             isComplete_;
	bool             running_;

	CFrame(unsigned winSize)
	:frameID_( NO_FRAME ),
	 oldestFrag_( 0 ),
	 lastFrag_( NO_FRAG ),
	 fragWin_( winSize ),
	 isComplete_( false ),
	 running_( false )
	{
	}

	bool isComplete()
	{
		return isComplete_;
	}

	void release(unsigned *ctr)
	{
	unsigned i;
		if ( ctr )
			(*ctr)++;
		prod_.reset();
		for ( i=0; i<fragWin_.size(); i++ )
			fragWin_[i].reset();
		frameID_    = NO_FRAME;
		lastFrag_   = NO_FRAG;
		oldestFrag_ = 0;
		isComplete_ = false;
		running_    = false;
	}

	void updateChain()
	{
	unsigned idx = oldestFrag_ & (fragWin_.size() - 1);
	unsigned l;
		while ( fragWin_[idx] ) {
			prod_->addAtTail( fragWin_[idx] );
			fragWin_[idx].reset();
			idx = (++oldestFrag_ & (fragWin_.size() - 1));
		}
		l = prod_->getLen();
		if ( l == lastFrag_ + 1 || CAxisFrameHeader::FRAG_MAX == l )
			isComplete_ = true;
	}

	friend class CProtoModDepack;
};

class CProtoModDepack : public CProtoMod {
private:
	unsigned badHeaderDrops_;
	unsigned oldFrameDrops_;
	unsigned newFrameDrops_;
	unsigned oldFragDrops_;
	unsigned newFragDrops_;
	unsigned duplicateFragDrops_;
	unsigned duplicateLastSeen_;
	unsigned noLastSeen_;
	unsigned fragsAccepted_;
	unsigned framesAccepted_;
	unsigned oqueueFullDrops_;
	unsigned evictedFrames_;
	unsigned incompleteDrops_;
	unsigned emptyDrops_;
	unsigned timedOutFrames_;
	unsigned pastLastDrops_;

	CTimeout timeout_;

	pthread_t tid_;
protected:
	unsigned frameWinSize_;
	unsigned fragWinSize_;

	FrameID        oldestFrame_;
	vector<CFrame> frameWin_;

	void processBuffer(Buf);

	void frameSync(CAxisFrameHeader *);

	void threadBody();

	void releaseFrames(bool onlyComplete);

	bool releaseOldestFrame(bool onlyComplete);

	void startTimeout(CFrame *frame);

	unsigned toFrameIdx(unsigned frameNo) { return frameNo & ( frameWinSize_ - 1 ); }
	unsigned toFragIdx(unsigned fragNo)   { return fragNo  & ( fragWinSize_  - 1 ); }

	static void *pthreadBody(void *);


public:
	CProtoModDepack(CProtoModKey k, CBufQueueBase::size_type oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned long timeoutUS);

	~CProtoModDepack();

	const char *getName() const { return "AXIS Depack"; }
};

#endif
