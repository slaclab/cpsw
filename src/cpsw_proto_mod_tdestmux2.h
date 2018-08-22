 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_TDEST_MUX_2_H
#define CPSW_PROTO_MOD_TDEST_MUX_2_H

#include <cpsw_proto_mod_bytemux.h>
#include <cpsw_event.h>
#include <boost/atomic.hpp>
#include <cpsw_proto_depack.h>
#include <cpsw_thread.h>
#include <vector>
#include <stdio.h>

using boost::atomic;
using boost::memory_order_acquire;
using boost::memory_order_release;

class CProtoModTDestMux2;
typedef shared_ptr<CProtoModTDestMux2>  ProtoModTDestMux2;
class CTDestPort2;
typedef shared_ptr<CTDestPort2>         TDestPort2;

class CTDestMuxer2Thread : public CRunnable {
private:
	CProtoModTDestMux2 *owner_;
public:
	CTDestMuxer2Thread(CProtoModTDestMux2 *owner, int prio = DFLT_PRIORITY);
	CTDestMuxer2Thread(const CTDestMuxer2Thread &, CProtoModTDestMux2 *new_owner);

	virtual void *threadBody();
};

class CProtoModTDestMux2 : public CProtoModByteMux<TDestPort2>, public CNoopEventHandler {
public:

	struct Work {
	public:
		BufChain      bc_;             // chain currently being fragmented on the way upstream	
		FragID        fragNo_;         // current fragment index
		int           tdest_;          // port for which we are working
		atomic<int>   inpQueueFill_;   // count of elements currently in the input queue
		bool          stripHeader_;    // cached value
		uint32_t      crc_;

		Work()
		: inpQueueFill_(0)
		{
		}

		void reset(BufChain bc)
		{
			bc_     = bc;
			fragNo_ = 0;
			crc_    = -1;
		}
	};

private:

	EventSet           inputDataAvailable_;
	Work               work_[DEST_MAX-DEST_MIN+1];
	unsigned           numWork_;
	unsigned long      goodTxFragCnt_;
	unsigned long      goodTxFramCnt_;
	unsigned long      badHeadersCnt_;
	unsigned           myMTUCached_;
	
	CTDestMuxer2Thread muxer_;

protected:
	CProtoModTDestMux2(const CProtoModTDestMux2 &orig, Key &k)
	: CProtoModByteMux<TDestPort2>(orig, k),
	  inputDataAvailable_( IEventSet::create() ),
	  numWork_           ( 0                   ),
	  goodTxFragCnt_     ( 0                   ),
	  goodTxFramCnt_     ( 0                   ),
	  badHeadersCnt_     ( 0                   ),
	  myMTUCached_       ( 0                   ),
	  muxer_             ( orig.muxer_         )
	{
	}

	TDestPort2 newPort(int dest, bool stripHeader, unsigned oQDepth, unsigned iQDepth)
	{
		// don't add to event set during creation but from 'add' -- this facilitates cloning
		return CShObj::create<TDestPort2>( getSelfAs<ProtoModTDestMux2>(), dest, stripHeader, oQDepth, iQDepth );
	}

public:

	// send one fragment, return true if it was the last one
	bool
	sendFrag(unsigned current);

	// do the work
	virtual void process();

	virtual void postWork(unsigned slot)
	{
		work_[slot].inpQueueFill_.fetch_add( 1, memory_order_release );
	}

	CProtoModTDestMux2(Key &k, int threadPriority)
	: CProtoModByteMux<TDestPort2>(k, "TDEST VC Demux V2", threadPriority),
	  inputDataAvailable_( IEventSet::create()  ),
	  numWork_           ( 0                    ),
	  goodTxFragCnt_     ( 0                    ),
	  goodTxFramCnt_     ( 0                    ),
	  badHeadersCnt_     ( 0                    ),
	  muxer_             ( this, threadPriority )
	{
	}

	TDestPort2 createPort(int dest, bool stripHeader, unsigned oQDepth, unsigned iQDepth)
	{
		return addPort( dest, newPort(dest, stripHeader, oQDepth, iQDepth) );
	}

	virtual TDestPort2 addPort(int dest, TDestPort2 port);

	virtual int extractDest(BufChain);

	virtual CProtoModTDestMux2 *clone(Key k)
	{
		return new CProtoModTDestMux2( *this, k );
	}

	virtual const char *getName() const
	{
		return "TDEST Demultiplexer V2";
	}

	static unsigned getMTU(ProtoDoor);

	virtual void dumpInfo(FILE *f);

	virtual void modStartup();
	virtual void modShutdown();

	virtual ~CProtoModTDestMux2() {}
};

class CTDestPort2 : public CByteMuxPort<CProtoModTDestMux2> {
private:
	bool          stripHeader_;
	unsigned      inpQueueDepth_;
    BufQueue      inputQueue_;    // from downstream module
	unsigned      slot_;

	BufChain      assembleBuffer_;
	FragID        fragNo_;
	unsigned long badHeadersCnt_;
	unsigned long nonSeqFragCnt_;
	unsigned long goodRxFragCnt_;
	unsigned long goodRxFramCnt_;

protected:
	CTDestPort2(const CTDestPort2 &orig, Key k)
	: CByteMuxPort<CProtoModTDestMux2>(orig, k),
	  slot_         ( -1 ),
	  badHeadersCnt_(  0 ),
	  nonSeqFragCnt_(  0 ),
	  goodRxFragCnt_(  0 ),
	  goodRxFramCnt_(  0 )
	{
	}

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	CTDestPort2(Key &k, ProtoModTDestMux2 owner, int dest, bool stripHeader, unsigned oQDepth, unsigned iQDepth)
	: CByteMuxPort<CProtoModTDestMux2>(k, owner, dest, oQDepth),
	  stripHeader_  ( stripHeader                  ),
	  inpQueueDepth_( iQDepth                      ),
	  inputQueue_   ( IBufQueue::create( iQDepth ) ),
	  slot_         ( -1                           ),
	  badHeadersCnt_( 0                            ),
	  nonSeqFragCnt_( 0                            ),
	  goodRxFragCnt_( 0                            ),
	  goodRxFramCnt_( 0                            )
	{
	}

	virtual bool
	getStripHeader()
	{
		return stripHeader_;
	}

	virtual unsigned
	getInpQueueDepth() const
	{
		return inpQueueDepth_;
	}

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout);
	virtual bool tryPush(BufChain bc);

	virtual void dumpYaml(YAML::Node &) const;

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

	virtual IEventSource *getInputQueueReadEventSource()
	{
		return inputQueue_->getReadEventSource();
	}

	virtual BufChain tryPopInputQueue()
	{
		return inputQueue_->tryPop();
	}

	virtual void attach(unsigned slot)
	{
		slot_ = slot;
	}

	virtual CTDestPort2 *clone(Key k)
	{
		return new CTDestPort2( *this, k );
	}

	virtual unsigned getMTU();

	virtual void dumpInfo(FILE *f);
};

#endif
