 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_SRP_MUX_H
#define CPSW_PROTO_MOD_SRP_MUX_H

#include <cpsw_proto_mod_bytemux.h>
#include <cpsw_api_builder.h>

class CProtoModSRPMux;
typedef shared_ptr<CProtoModSRPMux>  ProtoModSRPMux;
class CSRPPort;
typedef shared_ptr<CSRPPort>         SRPPort;

class CProtoModSRPMux : public CProtoModByteMux<SRPPort> {
private:
	INetIODev::ProtocolVersion protoVersion_;

protected:
	CProtoModSRPMux(const CProtoModSRPMux &orig, Key &k)
	: CProtoModByteMux<SRPPort>(orig, k),
	  protoVersion_(orig.protoVersion_)
	{
	}

	SRPPort newPort(int dest);

public:

	CProtoModSRPMux(Key &k, INetIODev::ProtocolVersion protoVersion, int threadPriority)
	: CProtoModByteMux<SRPPort>(k, "SRP VC Demux", threadPriority),
	  protoVersion_(protoVersion)
	{
	}

	SRPPort createPort(int dest)
	{
		return addPort(dest, newPort(dest));
	}

	// range of bits in TID that the downstream user may occupy
	virtual int getTidLsb()
	{
		return 0;
	}

	virtual int getTidNumBits()
	{
		return 24;
	}

	virtual int extractDest(BufChain);


	virtual INetIODev::ProtocolVersion getProtoVersion()
	{
		return protoVersion_;
	}

	virtual CProtoModSRPMux *clone(Key k)
	{
		return new CProtoModSRPMux( *this, k );
	}

	virtual const char *getName() const
	{
		return "SRP VC Demux";
	}

	virtual ~CProtoModSRPMux() {}

};

class CSRPPort : public CByteMuxPort<CProtoModSRPMux> {
protected:
	CSRPPort(const CSRPPort &orig, Key k)
	: CByteMuxPort<CProtoModSRPMux>(orig, k)
	{
	}

	virtual BufChain processOutput(BufChain *bc);

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	CSRPPort(Key &k, ProtoModSRPMux owner, int dest)
	: CByteMuxPort<CProtoModSRPMux>(k, owner, dest, 1) // queue depth 1 is enough for synchronous operations
	{
	}

	virtual void dumpYaml(YAML::Node &) const;

	virtual INetIODev::ProtocolVersion getProtoVersion();

	virtual CSRPPort *clone(Key k)
	{
		return new CSRPPort( *this, k );
	}
};

#endif
