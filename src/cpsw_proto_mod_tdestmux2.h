 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_TDEST_MUX_H
#define CPSW_PROTO_MOD_TDEST_MUX_H

#include <cpsw_proto_mod_bytemux.h>
#include <cpsw_event.h>
#include <boost/atomic.hpp>
#include <cpsw_proto_depack.h>

using boost::atomic;
using boost::memory_order_acquire;
using boost::memory_order_release;

class CProtoModTDestMux2;
typedef shared_ptr<CProtoModTDestMux2>  ProtoModTDestMux2;
class CTDestPort2;
typedef shared_ptr<CTDestPort2>         TDestPort2;

class CProtoModTDestMux2 : public CProtoModByteMux<TDestPort2>, public IEventHandler {
private:

	EventSet inputDataAvailable_;

protected:
	CProtoModTDestMux2(const CProtoModTDestMux2 &orig, Key &k)
	: CProtoModByteMux<TDestPort2>(orig, k),
	  inputDataAvailable_( IEventSet::create() )
	{
	}

	TDestPort2 newPort(int dest, bool stripHeader, unsigned oQDepth, unsigned iQDepth)
	{
		// don't add to event set during creation but from 'add' -- this facilitates cloning
		return CShObj::create<TDestPort2>( getSelfAs<ProtoModTDestMux2>(), dest, stripHeader, oQDepth, iQDepth );
	}

public:

	CProtoModTDestMux2(Key &k, int threadPriority)
	: CProtoModByteMux<TDestPort2>(k, "TDEST VC Demux V2", threadPriority),
	  inputDataAvailable_( IEventSet::create() )
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

	virtual ~CProtoModTDestMux2() {}
};

class CTDestPort2 : public CByteMuxPort<CProtoModTDestMux2> {
private:
	bool          stripHeader_;
    BufQueue      inputQueue_;    // from downstream module
	atomic<int>   inpQueueFill_;  // count of elements currently in the input queue
	BufChain      upstreamWork_;  // chain currently being fragmented on the way upstream	
	Buf           currentFrag_;   // current fragment
	FragID        fragNumber_;    // current fragment index

protected:
	CTDestPort2(const CTDestPort2 &orig, Key k)
	: CByteMuxPort<CProtoModTDestMux2>(orig, k)
	{
	}

	virtual BufChain processOutput();

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	CTDestPort2(Key &k, ProtoModTDestMux2 owner, int dest, bool stripHeader, unsigned oQDepth, unsigned iQDepth)
	: CByteMuxPort<CProtoModTDestMux2>(k, owner, dest, oQDepth),
	  stripHeader_ ( stripHeader                  ),
	  inputQueue_  ( IBufQueue::create( iQDepth ) ),
      inpQueueFill_( 0                            ),
      fragNumber_  ( 0                            )
	{
	}

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout);
	virtual bool tryPush(BufChain bc);

	virtual void dumpYaml(YAML::Node &) const;

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

	virtual IEventSource *getInputQueueReadEventSource()
	{
		return inputQueue_->getReadEventSource();
	}

	virtual CTDestPort2 *clone(Key k)
	{
		return new CTDestPort2( *this, k );
	}

	virtual unsigned getMTU();
};

#endif
