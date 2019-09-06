 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS

#include <cpsw_comm_addr.h>
#include <cpsw_yaml.h>

void CCommAddressImpl::dump(FILE *f) const
{
	getOwner()->dump(f);
	CAddressImpl::dump(f);
	fprintf(f,"\nProtocol Modules:\n");
	if ( protoStack_ ) {
		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			m->dumpInfo( f );
		}
	}
}

void
CCommAddressImpl::dumpYamlPart( YAML::Node &node) const
{
ProtoPort port;
	CAddressImpl::dumpYamlPart( node );
	// since SRP V2 is the default we switch it OFF here; if SRP
	// is chained after us they will re-enable
	YAML::Node noSRP;
	writeNode(noSRP, YAML_KEY_protocolVersion, IProtoStackBuilder::SRP_UDP_NONE);
	writeNode(node , YAML_KEY_SRP,             noSRP                  );
	for ( port = protoStack_; port; port = port->getUpstreamPort() ) {
		port->dumpYaml( node );
	}
}

int
CCommAddressImpl::open(CompositePathIterator *node)
{
CMtx::lg guard( & doorMtx_ );

int rval = CAddressImpl::open( node );

	if ( 0 == rval ) {
		door_ = getProtoStack()->open();
	}

	return rval;
}

int
CCommAddressImpl::close(CompositePathIterator *node)
{
CMtx::lg guard( & doorMtx_ );

int rval = CAddressImpl::close( node );

	if ( 1 == rval )
		door_.reset();

	return rval;
}

uint64_t
CCommAddressImpl::read(CReadArgs *args) const
{
BufChain bch;

	if ( 0 == args->dst_ ) {
		// They just want to wait until data arrives (or a timeout occurs) and
		// then do the actual 'read' afterwards
		EventSet          evSet = IEventSet::create();
		CNoopEventHandler handler;

		evSet->add( door_->getReadEventSource(), &handler );

		bool dataReady;

		if ( args->timeout_.isNone() ) {
			dataReady = evSet->processEvent( false, NULL );
		} else if ( args->timeout_.isIndefinite() ) {
			dataReady = evSet->processEvent( true , NULL );
		} else {
			CTimeout absTimeout;
			evSet->getAbsTimeout( &absTimeout, &args->timeout_ );
			dataReady = evSet->processEvent( true , &absTimeout );
		}
		// we can't peek at the buffer and thus don't know how much
		// data would be available but certainly at least 1 byte...
		return dataReady ? 1 : 0;
	}

	bch = door_->pop( &args->timeout_, IProtoPort::REL_TIMEOUT  );

	if ( ! bch )
		return 0;

	return bch->extract( args->dst_, args->off_, args->nbytes_ );
}

uint64_t
CCommAddressImpl::write(CWriteArgs *args) const
{
BufChain bch = IBufChain::create();
uint64_t rval;

	bch->insert( args->src_, args->off_, args->nbytes_, mtu_ );

	rval = bch->getSize();

	if ( ! door_->push( bch, &args->timeout_, IProtoPort::REL_TIMEOUT ) )
		return 0;
	return rval;
}

void CCommAddressImpl::startProtoStack()
{
	// start in reverse order
	if ( protoStack_  && ! running_) {
		std::vector<ProtoMod> mods;
		int                      i;

		running_ = true;

		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			mods.push_back( m );
		}
		for ( i = mods.size() - 1; i >= 0; i-- ) {
			mods[i]->modStartupOnce();
		}
	}
	mtu_ = protoStack_->open()->getMTU();
}

void CCommAddressImpl::shutdownProtoStack()
{
	if ( protoStack_ && running_ ) {
		ProtoMod m;
		for ( m = protoStack_->getProtoMod(); m; m=m->getUpstreamProtoMod() ) {
			m->modShutdownOnce();
		}
		running_ = false;
	}
}

CCommAddressImpl::~CCommAddressImpl()
{
	shutdownProtoStack();
}

