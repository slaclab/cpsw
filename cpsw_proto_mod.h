 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_H
#define CPSW_PROTO_MOD_H

#include <cpsw_api_user.h>

#include <boost/weak_ptr.hpp>
#include <boost/atomic.hpp>
#include <stdio.h>

#include <cpsw_buf.h>
#include <cpsw_shared_obj.h>
#include <cpsw_event.h>

using boost::weak_ptr;

class IProtoPort;
typedef shared_ptr<IProtoPort> ProtoPort;

class IProtoDoor;
typedef shared_ptr<IProtoDoor> ProtoDoor;

class IProtoMod;
typedef shared_ptr<IProtoMod> ProtoMod;

class CPortImpl;
typedef shared_ptr<CPortImpl> ProtoPortImpl;

class ProtoPortMatchParams;

namespace YAML {
	class Node;
}

class IProtoPort {
public:

	static const bool         ABS_TIMEOUT = true;
	static const bool         REL_TIMEOUT = false;

	// An 'open' ProtoPort
	virtual ProtoDoor open()                                 = 0;

	virtual ProtoMod  getProtoMod()                          = 0;
	virtual ProtoDoor getUpstreamDoor()                      = 0;
	// use getUpstreamPort() if you don't need an 'open door'.
	// This ensures the door is not kept open unnecessarily
	virtual ProtoPort getUpstreamPort()                      = 0;

	virtual void      addAtPort(ProtoMod downstreamMod)      = 0;

	virtual IEventSource *getReadEventSource()               = 0;

	virtual CTimeout  getAbsTimeoutPop (const CTimeout *)    = 0;
	virtual CTimeout  getAbsTimeoutPush(const CTimeout *)    = 0;

	virtual void      dumpYaml(YAML::Node &)     const       = 0;

	virtual int       match(ProtoPortMatchParams*)           = 0;

	virtual ~IProtoPort()
	{
	}
};

/* Interface which should be implemented by classes which use
 * protocol stacks. It helps finding an existing stack which
 * matches select parameters so that new ports can be created.
 * E.g., someone could be trying to create a new port for
 * a certain UDPPort/TDEST combination. 
 * 'findProtoPort()' could be used to locate an existing TDEST
 * multiplexer on the desired UDP port so that a new TDEST
 * port can then be added there.
 */
class IProtoPortLocator {
public:
	virtual ProtoPort findProtoPort(ProtoPortMatchParams *) = 0;

	virtual ~IProtoPortLocator(){}
};

class IProtoDoor : public IProtoPort {
public:
	// returns NULL shared_ptr on timeout; throws on error
	virtual BufChain pop(const CTimeout *, bool abs_timeout) = 0;
	virtual BufChain tryPop()                                = 0;

	// Successfully pushed buffers are unlinked from the chain
	virtual bool push(BufChain , const CTimeout *, bool abs_timeout) = 0;
	virtual bool tryPush(BufChain)                           = 0;

	// To 'close' a 'Door' interface (and continue using
	// the 'Port' base interface) hold on to the return
	// value and reset the 'Door' pointer:
	//
	//  ProtoPort demoted = door->close();
	//                      door.reset();
	//
	virtual ProtoPort close()                                = 0;
};

// find a protocol stack based on parameters
class ProtoPortMatchParams {
public:
	class MatchParam {
	public:
		ProtoPort matchedBy_;
		bool      doMatch_;
		bool      exclude_;
		ProtoMod  handledBy_;
		MatchParam(bool doMatch = false)
		: doMatch_(doMatch),
		  exclude_(false)
		{
		}
		void exclude()
		{
			doMatch_ = true;
			exclude_ = true;
		}
		void include()
		{
			doMatch_ = true;
			exclude_ = false;
		}

		int excluded()
		{
			return doMatch_ && exclude_ && ! handledBy_ ? 1 : 0;
		}

		void reset()
		{
			matchedBy_.reset();
			handledBy_.reset();
		}
	};

	class MatchParamUnsigned : public MatchParam {
	protected:
		unsigned val_;
		bool     any_;
	public:
		MatchParamUnsigned(unsigned val = (unsigned)-1, bool doMatch = false)
		: MatchParam( doMatch ? true : val != (unsigned)-1 ),
		  val_(val),
		  any_(false)
		{
		}

		void wildcard()
		{
			any_     = true;
			include();
		}

		MatchParamUnsigned & operator=(unsigned val)
		{
			val_     = val;
			include();
			any_     = false;
			return *this;
		}

		bool operator==(unsigned val)
		{
			return doMatch_ && (any_ || (val_ == val));
		}

	};
	MatchParamUnsigned tcpDestPort_, udpDestPort_, srpVersion_, srpVC_, tDest_;
	MatchParam         haveRssi_, haveDepack_;

	void reset()
	{
		tcpDestPort_.reset();
		udpDestPort_.reset();
		srpVersion_.reset();
		srpVC_.reset();
		tDest_.reset();
		haveRssi_.reset();
		haveDepack_.reset();
	}

	int requestedMatches()
	{
	int rval = 0;
		if ( tcpDestPort_.doMatch_ )
			rval++;
		if ( udpDestPort_.doMatch_ )
			rval++;
		if ( haveDepack_.doMatch_ )
			rval++;
		if ( srpVersion_.doMatch_ )
			rval++;
		if ( srpVC_.doMatch_ )
			rval++;
		if ( haveRssi_.doMatch_ )
			rval++;
		if ( tDest_.doMatch_ )
			rval++;
		return rval;
	}

	int excluded()
	{
		return
			  tcpDestPort_.excluded()
			+ udpDestPort_.excluded()
			+ haveDepack_.excluded()
			+ srpVersion_.excluded()
			+ srpVC_.excluded()
			+ haveRssi_.excluded()
			+ tDest_.excluded();
	}

	int findMatches(ProtoPort p)
	{
		int rval  = p->match( this );
			rval += excluded();

		return rval;
	}
};


class IProtoMod {
public:
	// to be called by the upstream module's addAtPort() method
	// (which is protocol specific)
	virtual void attach(ProtoDoor upstream)            = 0;

	virtual ProtoDoor getUpstreamDoor()                = 0;
	virtual ProtoMod  getUpstreamProtoMod()            = 0;

	virtual void dumpInfo(FILE *)                      = 0;

	virtual void modStartup()                          = 0;
	virtual void modShutdown()                         = 0;

	virtual const char *getName() const                = 0;

	virtual ~IProtoMod() {}
};

class IPortImpl : public IProtoDoor {
private:
	boost::atomic<bool>    spinlock_;
	ProtoPort              forcedSelfReference_;
    boost::atomic<int>     openCount_;

protected:
	virtual ProtoPort getSelfAsProtoPort()        = 0;

public:
	virtual int iMatch(ProtoPortMatchParams *cmp) = 0;

public:
	IPortImpl();

	virtual int match(ProtoPortMatchParams *cmp);

	virtual ProtoDoor open();

	virtual ProtoPort close();

	virtual ProtoPort getUpstreamPort();

	virtual int isOpen() const;

	friend class CloseManager;
};

class CPortImpl : public IPortImpl {
private:
	weak_ptr< ProtoMod::element_type > downstream_;
	BufQueue                           outputQueue_;
	unsigned                           depth_;

protected:

	CPortImpl(const CPortImpl &orig)
	{
		// would have to set downstream_ to
		// the respective clone and clone
		// the output queue...
		throw InternalError("clone not implemented");
	}

	virtual BufChain processOutput(BufChain bc)
	{
		throw InternalError("processOutput() not implemented!\n");
	}

	virtual BufChain processInput(BufChain bc)
	{
		throw InternalError("processInput() not implemented!\n");
	}


public:

	CPortImpl(unsigned n);

	virtual unsigned getQueueDepth() const;

	virtual IEventSource *getReadEventSource();

	virtual void      addAtPort(ProtoMod downstream);

	virtual bool pushDownstream(BufChain bc, const CTimeout *rel_timeout);

	// getAbsTimeout is not a member of the CTimeout class:
	// the clock to be used is implementation dependent.
	// ProtoMod uses a semaphore which uses CLOCK_REALTIME.
	// The conversion to abs-time should be a member
	// of the same class which uses the clock-dependent
	// resource...
	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout);

	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout);

	virtual BufChain pop(const CTimeout *, bool abs_timeout);
	virtual BufChain tryPop();

	// Successfully pushed buffers are unlinked from the chain
	virtual bool push(BufChain , const CTimeout *, bool abs_timeout);
	virtual bool tryPush(BufChain);

	virtual ProtoDoor getUpstreamDoor();

	virtual ProtoDoor mustGetUpstreamDoor();

	virtual ~CPortImpl()
	{
	}
};

class CProtoModImpl : public IProtoMod {
protected:
	ProtoDoor  upstream_;

	CProtoModImpl(const CProtoModImpl &orig);

public:
	CProtoModImpl();

	// subclasses may want to start threads
	// once the module is attached
	virtual void modStartup();

	virtual void modShutdown();

	virtual void attach(ProtoDoor upstream);

	virtual ProtoDoor getUpstreamDoor();
	virtual ProtoMod  getUpstreamProtoMod();

	virtual void dumpInfo(FILE *f);
};

// protocol module with single downstream port
class CProtoMod : public CShObj, public CProtoModImpl, public CPortImpl {

protected:
	CProtoMod(const CProtoMod &orig, Key &k);

	virtual ProtoPort getSelfAsProtoPort();

public:
	CProtoMod(Key &k, unsigned n);

	virtual bool pushDown(BufChain bc, const CTimeout *rel_timeout);

public:

	virtual ProtoMod getProtoMod();

	virtual ~CProtoMod()
	{
	}
};

#endif
