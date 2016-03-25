#ifndef CPSW_PROTO_MOD_DEPACK_H
#define CPSW_PROTO_MOD_DEPACK_H

#include <boost/intrusive/list.hpp>
#include <boost/atomic.hpp>
#include <vector>

#include <cpsw_api_user.h>
#include <cpsw_proto_mod.h>
#include <cpsw_thread.h>

#include <pthread.h>

using boost::atomic;
using boost::memory_order_relaxed;
using std::vector;

typedef unsigned FrameID;
typedef unsigned FragID;
typedef unsigned ProtoVersion;

class CProtoModDepack;
typedef shared_ptr<CProtoModDepack> ProtoModDepack;

class IProtoModDepack : public virtual IProtoMod {
};

class CAxisFrameHeader {
private:
	FrameID      frameNo_;
	FragID       fragNo_;
	ProtoVersion vers_;
	uint8_t      tDest_;
	uint8_t      tId_;
	uint8_t      tUsr1_;

public:
	static const unsigned VERSION_BIT_OFFSET  =  0;
	static const unsigned VERSION_BIT_SIZE    =  4;
	static const unsigned FRAME_NO_BIT_OFFSET =  4;
	static const unsigned FRAME_NO_BIT_SIZE   = 12;
	static const unsigned FRAG_NO_BIT_OFFSET  = 16; 
	static const unsigned FRAG_NO_BIT_SIZE    = 24; 
	static const unsigned FRAG_MAX            = (1<<FRAG_NO_BIT_SIZE) - 1;

	static const unsigned TDEST_BIT_OFFSET    = 40;
	static const unsigned TDEST_BIT_SIZE      =  8;
	static const unsigned TID_BIT_OFFSET      = 40;
	static const unsigned TID_BIT_SIZE        =  8;
	static const unsigned TUSR1_BIT_OFFSET    = 56;
	static const unsigned TUSR1_BIT_SIZE      =  8;

	static const unsigned FRAG_LAST_BIT       =  7;

	static const unsigned VERSION_0           =  0;

	static const unsigned V0_HEADER_SIZE      =  8;

	static const unsigned V0_TAIL_SIZE        =  1;

	class CAxisFrameNoAllocator {
	private:
		atomic<FrameID> frameNo_;
	public:
		CAxisFrameNoAllocator()
		:frameNo_(0)
		{
		}

		FrameID newFrameID()
		{
		FrameID id;
		unsigned shft = (8*sizeof(FrameID)-FRAME_NO_BIT_SIZE);
			id = frameNo_.fetch_add( (1<<shft), memory_order_relaxed ) >> shft;
			return id & ((1<<FRAME_NO_BIT_SIZE) - 1);
		}
	};

	CAxisFrameHeader(unsigned frameNo = 0, unsigned fragNo = 0, unsigned tDest = 0)
	:frameNo_(frameNo),
	 fragNo_(fragNo),
	 vers_(VERSION_0),
	 tDest_( tDest ),
	 tId_(0),
	 tUsr1_(0)
	{
	}

	bool parse(uint8_t *hdrBase, size_t hdrSize);

	class InvalidHeaderException {};

	CAxisFrameHeader(uint8_t *hdrBase, size_t hdrSize)
	{
		if ( ! parse(hdrBase, hdrSize) )
			throw InvalidHeaderException();
	}
	
	void insert(uint8_t *hdrBase, size_t hdrSize);

	FrameID      getFrameNo() { return frameNo_; }
	FragID       getFragNo()  { return fragNo_;  }
	ProtoVersion getVersion() { return vers_;    }
	uint8_t      getTDest()   { return tDest_;   }

	void         setFrameNo(FrameID frameNo)
	{
		frameNo_ = frameNo;
	}

	void         setFragNo(FragID   fragNo)
	{
		fragNo_  = fragNo;
	}

	void         setTDest(uint8_t   tDest)
	{
		tDest_   = tDest;
	}

	static FrameID moduloFrameSz(FrameID id)
	{
		return (id & ((1<<FRAME_NO_BIT_SIZE) - 1));
	}

	static int   signExtendFrameNo(FrameID x)
	{
		if ( x & (1<<(FRAME_NO_BIT_SIZE-1)) )
			 x |= ~((1<<FRAME_NO_BIT_SIZE)-1);
		return x;
	}

	size_t       getSize()     { return V0_HEADER_SIZE; }

	size_t       getTailSize() { return V0_TAIL_SIZE;   }

	static bool getTailEOF(uint8_t *tailbuf)           { return (*tailbuf) & (1<<FRAG_LAST_BIT);    }
	static void setTailEOF(uint8_t *tailbuf, bool eof) { (*tailbuf) = eof ? (1<<FRAG_LAST_BIT) : 0; }

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

class CProtoModDepack : public CProtoMod, public CRunnable {
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

	CAxisFrameHeader::CAxisFrameNoAllocator frameIdGen_;

protected:
	unsigned frameWinSize_;
	unsigned fragWinSize_;

	FrameID        oldestFrame_;
	vector<CFrame> frameWin_;

	virtual void processBuffer(Buf);

	virtual void frameSync(CAxisFrameHeader *);

	virtual void* threadBody();

	virtual void modStartup();
	virtual void modShutdown();

	virtual void releaseFrames(bool onlyComplete);

	virtual bool releaseOldestFrame(bool onlyComplete);

	virtual BufChain processOutput(BufChain bc);

	virtual void startTimeout(CFrame *frame);

	virtual unsigned toFrameIdx(unsigned frameNo) { return frameNo & ( frameWinSize_ - 1 ); }
	virtual unsigned toFragIdx(unsigned fragNo)   { return fragNo  & ( fragWinSize_  - 1 ); }

	CProtoModDepack( CProtoModDepack &orig, Key &k );

public:
	CProtoModDepack(Key &k, unsigned oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, uint64_t timeoutUS);

	virtual ~CProtoModDepack();

	virtual void dumpInfo(FILE *f);

	virtual CProtoModDepack * clone(Key &k) { return new CProtoModDepack( *this, k ); }

	virtual const char *getName() const { return "AXIS Depack"; }
};

#endif
