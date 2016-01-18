#ifndef CPSW_PROTO_MOD_SRP_H
#define CPSW_PROTO_MOD_SRP_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_api_builder.h>

class CProtoModSRPMux;
typedef shared_ptr<CProtoModSRPMux> ProtoModSRPMux;
class CSRPPort;
typedef shared_ptr<CSRPPort>     SRPPort;

class CProtoModSRPMux : public CShObj, public CProtoModImpl {

public:
	const static int VC_MIN = 0;   // the code relies on VC occupying 1 byte
	const static int VC_MAX = 255;

private:

	weak_ptr<SRPPort::element_type> downstream_[VC_MAX-VC_MIN+1];

	
	INoSsiDev::ProtocolVersion protoVersion_;	
	

protected:
	ProtoPort upstream_;

	CProtoModSRPMux(const CProtoModSRPMux &orig, Key &k)
	: CShObj(k),
	  protoVersion_(orig.protoVersion_)
	{
		throw InternalError("Clone not implemented");
	}

public:
	CProtoModSRPMux(Key &k, INoSsiDev::ProtocolVersion protoVersion)
	: CShObj(k),
	  protoVersion_(protoVersion)
	{
	}

	virtual INoSsiDev::ProtocolVersion getProtoVersion()
	{
		return protoVersion_;
	}

	// in case there is no downstream module
	virtual SRPPort createPort(int vc);

	virtual bool pushDown(BufChain bc);

	virtual SRPPort findPort(int vc)
	{
		if ( vc < VC_MIN || vc > VC_MAX )
			throw InvalidArgError("Virtual channel number out of range");
		if ( downstream_[ vc - VC_MIN ].expired() )
			return SRPPort();
		return SRPPort( downstream_[ vc - VC_MIN ] );
	}

	virtual CProtoModSRPMux *clone(Key k) { return new CProtoModSRPMux( *this, k ); }

	virtual ~CProtoModSRPMux()
	{
	}
};

class CSRPPort : public CShObj, public CPortImpl {
private:
	int                                vc_;
	// we use strong pointers in the upstream direction
	// and the 'owner' represents this.
	// Thus the 'downstream' vector in the protocol
	// module must host weak pointers
	ProtoModSRPMux                     owner_;
	weak_ptr< ProtoMod::element_type > downstream_;

protected:
	CSRPPort(const CSRPPort &orig, Key k)
	: CShObj(k),
	  CPortImpl(orig)
	{
	}

	virtual ProtoPort getSelfAsProtoPort()
	{
		return getSelfAs<SRPPort>();
	}

	virtual BufChain processOutput(BufChain bc);

public:
	CSRPPort(Key &k, ProtoModSRPMux owner, int vc)
	: CShObj(k),
	  CPortImpl(1), // queue depth 1 is enough for synchronous operations
      vc_(vc),
	  owner_(owner)
	{
	}

	int getVC()
	{
		return vc_;
	}

protected:

	friend class CProtoModSRPMux;

public:

	virtual ProtoMod getProtoMod()
	{
		return ProtoMod( owner_ );
	}

	virtual INoSsiDev::ProtocolVersion getProtoVersion()
	{
		return ProtoModSRPMux( owner_ )->getProtoVersion();
	}

	virtual void attach(ProtoMod m)
	{
		if ( ! downstream_.expired() ) {
			throw ConfigurationError("Already a module attached downstream");
		}
		m->attach( getSelfAs<SRPPort>() );
		downstream_ = m;
	}

	virtual ProtoPort getUpstreamPort()
	{
	ProtoMod  owner = getProtoMod();
	ProtoPort rval;
		if ( owner )
			rval = owner->getUpstreamPort();
		return rval;
	}

	virtual ~CSRPPort()
	{
	}
};

#endif
