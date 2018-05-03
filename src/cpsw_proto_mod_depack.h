 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_DEPACK_H
#define CPSW_PROTO_MOD_DEPACK_H

#include <vector>

#include <cpsw_api_user.h>
#include <cpsw_proto_mod.h>
#include <cpsw_thread.h>
#include <cpsw_proto_depack.h>

#include <pthread.h>

using std::vector;

class CProtoModDepack;
typedef shared_ptr<CProtoModDepack> ProtoModDepack;

class IProtoModDepack : public virtual IProtoMod {
};

class CFrame {
public:
	static const FrameID NO_FRAME = (FrameID)(1<<24); // so that abs(frame-NO_FRAME) never is inside any window
	static const FragID  NO_FRAG  = (FragID)-2; // so that NO_FRAG + 1 is not a valid ID
protected:
	BufChain         prod_;
	FragID           nFrags_;
	FrameID          frameID_;
	FragID           oldestFrag_;
	FragID           lastFrag_;
	CTimeout         timeout_;
	vector<BufChain> fragWin_;
	bool             isComplete_;
	bool             running_;

	CFrame(unsigned winSize)
	:nFrags_( 0 ),
	 frameID_( NO_FRAME ),
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

	void release(unsigned *ctr);
	void updateChain();

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

	unsigned cachedMTU_;

	static const unsigned SAFE_MTU = 1024; // assume this just always fits

	CTimeout timeout_;

	CAxisFrameHeader::CAxisFrameNoAllocator frameIdGen_;

protected:
	unsigned frameWinSize_;
	unsigned fragWinSize_;

	FrameID        oldestFrame_;
	vector<CFrame> frameWin_;

	virtual void processBuffer(BufChain);

	virtual void frameSync(CAxisFrameHeader *);

	virtual void* threadBody();

	virtual void modStartup();
	virtual void modShutdown();

	virtual void releaseFrames(bool onlyComplete);

	virtual bool releaseOldestFrame(bool onlyComplete);

	virtual bool fitsInMTU(unsigned sizeBytes);

	virtual BufChain processOutput(BufChain *bc);

	virtual void startTimeout(CFrame *frame);

	virtual void appendTailByte(BufChain, bool);

	virtual unsigned toFrameIdx(unsigned frameNo) { return frameNo & ( frameWinSize_ - 1 ); }
	virtual unsigned toFragIdx(unsigned fragNo)   { return fragNo  & ( fragWinSize_  - 1 ); }

	virtual int      iMatch(ProtoPortMatchParams *cmp);

	CProtoModDepack( CProtoModDepack &orig, Key &k );

public:
	CProtoModDepack(Key &k, unsigned oqueueDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, CTimeout timeout, int threadPrio);

	virtual ~CProtoModDepack();

	virtual void dumpInfo(FILE *f);

	virtual unsigned getMTU();

	virtual CProtoModDepack * clone(Key &k) { return new CProtoModDepack( *this, k ); }

	virtual const char *getName() const { return "AXIS Depack"; }

	// Successfully pushed buffers are unlinked from the chain
	virtual bool push(BufChain , const CTimeout *, bool abs_timeout);
	virtual bool tryPush(BufChain);

	virtual void dumpYaml(YAML::Node &node) const;
};

#endif
