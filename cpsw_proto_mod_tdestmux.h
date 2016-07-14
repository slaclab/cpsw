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

	CProtoModTDestMux(Key &k)
	: CProtoModByteMux<TDestPort>(k, "TDest VC Demux")
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
		return "TDest Demultiplexer";
	}

	virtual ~CProtoModTDestMux() {}
};

class CTDestPort : public CByteMuxPort {
private:
	bool stripHeader_;

protected:
	CTDestPort(const CTDestPort &orig, Key k)
	: CByteMuxPort(orig, k)
	{
	}

	virtual BufChain processOutput(BufChain bc);

	virtual int iMatch(ProtoPortMatchParams *cmp);


public:
	CTDestPort(Key &k, ProtoModTDestMux owner, int dest, bool stripHeader, unsigned qDepth)
	: CByteMuxPort(k, owner, dest, qDepth),
	  stripHeader_(stripHeader)
	{
	}

	virtual void dumpYaml(YAML::Node &) const;

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

};

#endif
