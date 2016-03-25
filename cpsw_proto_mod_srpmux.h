#ifndef CPSW_PROTO_MOD_SRP_H
#define CPSW_PROTO_MOD_SRP_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_api_builder.h>
#include <cpsw_thread.h>

class CProtoModSRPMux;
typedef shared_ptr<CProtoModSRPMux>  ProtoModSRPMux;
class CSRPPort;
typedef shared_ptr<CSRPPort>         SRPPort;

template <typename PORT, typename DERIVED> class CProtoModByteMux : public CShObj, public CProtoModImpl, CRunnable {

public:
	const static int DEST_MIN = 0;   // the code relies on 'dest' occupying 1 byte
	const static int DEST_MAX = 255;

private:

	weak_ptr<typename PORT::element_type> downstream_[DEST_MAX-DEST_MIN+1];

protected:
	ProtoPort upstream_;

	CProtoModByteMux(const CProtoModByteMux &orig, Key &k)
	: CShObj(k),
	  CRunnable(orig)
	{
		throw InternalError("Clone not implemented");
	}

	virtual PORT newPort(int dest) = 0;

public:
	CProtoModByteMux(Key &k, const char *name)
	: CShObj(k),
	  CRunnable(name)
	{
	}

	// in case there is no downstream module
	virtual PORT createPort(int dest)
	{
		if ( dest < DEST_MIN || dest > DEST_MAX )
			throw InvalidArgError("Virtual channel number out of range");

		if ( ! downstream_[dest].expired() ) {
			throw ConfigurationError("Virtual channel already in use");
		}

		PORT port = newPort(dest);

		downstream_[dest - DEST_MIN] = port;

		return port;
	}


	virtual int  extractDest(BufChain) = 0;

	virtual bool pushDown(BufChain bc, const CTimeout *rel_timeout)
	{
		int dest;

		if ( (dest = extractDest( bc )) < DEST_MIN )
			return false;

		if ( downstream_[dest].expired() )
			return false; // nothing attached to this port; drop

		return PORT( downstream_[dest] )->pushDownstream(bc, rel_timeout);
	}

	virtual void * threadBody()
	{
		ProtoPort up = getUpstreamPort();
		while ( 1 ) {
			BufChain bc = up->pop( NULL, true );

			// if a single VC's queue is full then this stalls
			// all traffic. The alternative would be dropping
			// the packet but that would defeat the purpose
			// of rssi.
			pushDown( bc, NULL );
		}
		return NULL;
	}


	virtual PORT findPort(int dest)
	{
		if ( dest < DEST_MIN || dest > DEST_MAX )
			throw InvalidArgError("Destination channel number out of range");
		if ( downstream_[ dest - DEST_MIN ].expired() )
			return PORT();
		return PORT( downstream_[ dest - DEST_MIN ] );
	}

	virtual void modStartup()
	{
		threadStart();
	}

	virtual void modShutdown()
	{
		threadStop();
	}

	virtual ~CProtoModByteMux()
	{
	}
};

class CProtoModSRPMux : public CProtoModByteMux<SRPPort,CProtoModSRPMux> {
private:
	INoSsiDev::ProtocolVersion protoVersion_;	

protected:
	CProtoModSRPMux(const CProtoModSRPMux &orig, Key &k)
	: CProtoModByteMux(orig, k),
  	  protoVersion_(orig.protoVersion_)
	{
	}

	virtual SRPPort newPort(int dest)
	{
 		return CShObj::create<SRPPort>( getSelfAs<ProtoModSRPMux>(), dest );
	}

public:

	CProtoModSRPMux(Key &k, INoSsiDev::ProtocolVersion protoVersion)
	: CProtoModByteMux(k, "SRP VC Demux"),
	  protoVersion_(protoVersion)
	{
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


	virtual INoSsiDev::ProtocolVersion getProtoVersion()
	{
		return protoVersion_;
	}

	virtual CProtoModSRPMux *clone(Key k) { return new CProtoModSRPMux( *this, k ); }

	virtual const char *getName() const
	{
		return "SRP VC Demux";
	}

};


class CByteMuxPort : public CShObj, public CPortImpl {
private:
	int                                dest_;
	// we use strong pointers in the upstream direction
	// and the 'owner' represents this.
	// Thus the 'downstream' vector in the protocol
	// module must host weak pointers
	ProtoMod                           owner_;
	weak_ptr< ProtoMod::element_type > downstream_;

protected:
	CByteMuxPort(const CByteMuxPort &orig, Key k)
	: CShObj(k),
	  CPortImpl(orig)
	{
	}

	virtual ProtoPort getSelfAsProtoPort()
	{
		return getSelfAs< shared_ptr<CByteMuxPort> >();
	}

public:
	CByteMuxPort(Key &k, ProtoMod owner, int dest, unsigned queueDepth)
	: CShObj(k),
	  CPortImpl(queueDepth),
      dest_(dest),
	  owner_(owner)
	{
	}

	int getDest()
	{
		return dest_;
	}

public:

	virtual ProtoMod getProtoMod()
	{
		return owner_;
	}

	virtual void attach(ProtoMod m)
	{
		if ( ! downstream_.expired() ) {
			throw ConfigurationError("Already a module attached downstream");
		}
		downstream_ = m;
		m->attach( getSelfAs< shared_ptr<CByteMuxPort> >() );
	}

	virtual ProtoPort getUpstreamPort()
	{
	ProtoMod  owner = getProtoMod();
	ProtoPort rval;
		if ( owner )
			rval = owner->getUpstreamPort();
		return rval;
	}

};

class CSRPPort : public CByteMuxPort {
protected:
	CSRPPort(const CSRPPort &orig, Key k)
	: CByteMuxPort(orig, k)
	{
	}

	virtual BufChain processOutput(BufChain bc);

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	CSRPPort(Key &k, ProtoModSRPMux owner, int dest)
	: CByteMuxPort(k, owner, dest, 1) // queue depth 1 is enough for synchronous operations
	{
	}

	virtual INoSsiDev::ProtocolVersion getProtoVersion()
	{
		return boost::static_pointer_cast<ProtoModSRPMux::element_type>( getProtoMod() )->getProtoVersion();
	}
};


#endif
