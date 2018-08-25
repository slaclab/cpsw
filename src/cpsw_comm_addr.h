 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_COMM_ADDRESS_H
#define CPSW_COMM_ADDRESS_H

#include <cpsw_proto_mod.h>
#include <cpsw_address.h>
#include <cpsw_mutex.h>

class CCommAddressImpl : public CAddressImpl {
protected:
	ProtoPort      protoStack_;
	bool           running_;
	ProtoDoor      door_;
	unsigned       mtu_;

private:
	CMtx           doorMtx_;

public:
	CCommAddressImpl(AKey k, ProtoPort protoStack)
	: CAddressImpl( k          ),
	  protoStack_ ( protoStack ),
	  running_    ( false      ),
	  mtu_        ( 0          )
	{
	}

	CCommAddressImpl(CCommAddressImpl &orig, AKey k)
	: CAddressImpl(orig, k)
	{
		throw InternalError("Clone not implemented"); /* need to clone protocols, ... */
	}

	ProtoPort
	getProtoStack() const
	{
		return protoStack_;
	}

	// any subclass must implement 'clone'
	virtual CCommAddressImpl *clone(AKey k)
	{
		return new CCommAddressImpl( *this, k );
	}

	virtual int      open (CompositePathIterator *node);
	virtual int      close(CompositePathIterator *node);
	virtual uint64_t read (CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	virtual ~CCommAddressImpl();

	virtual void startProtoStack();
	virtual void shutdownProtoStack();
	virtual void dumpYamlPart(YAML::Node &) const;
};
#endif
