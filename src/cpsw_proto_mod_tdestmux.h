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

class CProtoModTDestMux;
typedef shared_ptr<CProtoModTDestMux>  ProtoModTDestMux;
class CTDestPort;
typedef shared_ptr<CTDestPort>         TDestPort;

class CProtoModTDestMux : public CProtoModByteMux<TDestPort> {
protected:
	CProtoModTDestMux(const CProtoModTDestMux &orig, Key &k)
	: CProtoModByteMux<TDestPort>(orig, k)
	{
	}

	TDestPort newPort(int dest, bool stripHeader, unsigned qDepth)
	{
		return CShObj::create<TDestPort>( getSelfAs<ProtoModTDestMux>(), dest, stripHeader, qDepth );
	}

public:

	CProtoModTDestMux(Key &k, int threadPriority)
	: CProtoModByteMux<TDestPort>(k, "TDEST VC Demux", threadPriority)
	{
	}

	TDestPort createPort(int dest, bool stripHeader, unsigned qDepth)
	{
		return addPort( dest, newPort(dest, stripHeader, qDepth) );
	}

	virtual int extractDest(BufChain);

	virtual CProtoModTDestMux *clone(Key k)
	{
		return new CProtoModTDestMux( *this, k );
	}

	virtual const char *getName() const
	{
		return "TDEST Demultiplexer";
	}

	virtual ~CProtoModTDestMux() {}
};

class CTDestPort : public CByteMuxPort<CProtoModTDestMux> {
private:
	bool stripHeader_;

protected:
	CTDestPort(const CTDestPort &orig, Key k)
	: CByteMuxPort<CProtoModTDestMux>(orig, k),
	  stripHeader_( orig.stripHeader_ )
	{
	}

	virtual BufChain processOutput(BufChain *bc);

	virtual int iMatch(ProtoPortMatchParams *cmp);


public:
	CTDestPort(Key &k, ProtoModTDestMux owner, int dest, bool stripHeader, unsigned qDepth)
	: CByteMuxPort<CProtoModTDestMux>(k, owner, dest, qDepth),
	  stripHeader_(stripHeader)
	{
	}

	virtual void dumpYaml(YAML::Node &) const;

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

	virtual CTDestPort *clone(Key k)
	{
		return new CTDestPort( *this, k );
	}
};

#endif
