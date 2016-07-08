#ifndef CPSW_PROTO_MOD_BYTE_MUX_H
#define CPSW_PROTO_MOD_BYTE_MUX_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_thread.h>

// Protocol demultiplexer with a max. of 256 'virtual-channels'

template <typename PORT> class CProtoModByteMux : public CShObj, public CProtoModImpl, CRunnable {

public:
	const static int DEST_MIN = 0;   // the code relies on 'dest' occupying 1 byte
	const static int DEST_MAX = 255;

private:

	weak_ptr<typename PORT::element_type> downstream_[DEST_MAX-DEST_MIN+1];
	unsigned nPorts_;

protected:
	ProtoPort upstream_;

	CProtoModByteMux(const CProtoModByteMux &orig, Key &k)
	: CShObj(k),
	  CProtoModImpl(orig),
	  CRunnable(orig),
	  nPorts_(orig.nPorts_)
	{
		throw InternalError("Clone not implemented");
	}

	virtual PORT addPort(int dest, PORT port)
	{
		if ( dest < DEST_MIN || dest > DEST_MAX )
			throw InvalidArgError("Virtual channel number out of range");

		if ( ! downstream_[dest].expired() ) {
			throw ConfigurationError("Virtual channel already in use");
		}

		downstream_[dest - DEST_MIN] = port;
		nPorts_++;

		return port;
	}

public:
	CProtoModByteMux(Key &k, const char *name)
	: CShObj(k),
	  CRunnable(name),
	  nPorts_(0)
	{
	}

	virtual const char *getName()      const = 0;

	virtual unsigned getNumPortsUsed() const
	{
		return nPorts_;
	}

	virtual void dumpInfo(FILE *f)
	{
		fprintf(f,"%s:\n", getName());
		fprintf(f,"  # virtual channels in use: %u\n", getNumPortsUsed());
	}

	// derived class usually implements a 'newPort' method
	// taking 'dest' and additional args 'args':
	//     PORT newPort(dest, args...)
	// it also should provide
	//     PORT createPort(dest, args...)
	//     {
	//         return addPort(dest, newPort(dest, args...));
	//     }

	// subclass must know how to extract the virtual-channel info
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

	int getDest() const
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

#endif
