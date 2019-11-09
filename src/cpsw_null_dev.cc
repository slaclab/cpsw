 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_null_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cpsw_stdio.h>

#include <cpsw_yaml.h>

/* A 'null' device which does nothing. Intended for testing + debugging so you can load
 * an arbitrary YAML file and explore the hierarchy etc.
 */

//#define NULLDEV_DEBUG

CNullDevImpl::CNullDevImpl(Key &k, const char *name, uint64_t size)
: CDevImpl(k, name, size)
{
}

CNullDevImpl::CNullDevImpl(Key &key, YamlState &node)
: CDevImpl(key, node)
{
}


CNullDevImpl::CNullDevImpl(const CNullDevImpl &orig, Key &k)
: CDevImpl(orig, k)
{
}

CNullDevImpl::~CNullDevImpl()
{
}

void CNullDevImpl::addAtAddress(Field child)
{
IAddress::AKey k = getAKey();

	add( cpsw::make_shared<CNullAddressImpl>(k), child );
}

void CNullDevImpl::addAtAddress(Field child, unsigned nelms)
{
	if ( 1 != nelms )
		throw ConfigurationError("CNullDevImpl::addAtAddress -- can only have exactly 1 child");
	addAtAddress( child );
}

CNullAddressImpl::CNullAddressImpl(AKey k)
: CAddressImpl(k, 1)
{
}

CNullAddressImpl::CNullAddressImpl(AKey k, YamlState &ypath)
: CAddressImpl(k, ypath)
{
	if ( 1 != getNelms() )
		throw ConfigurationError("CNullAddressImpl -- can only have exactly 1 child");
}

uint64_t CNullAddressImpl::read(CReadArgs *args) const
{
NullDevImpl owner( getOwnerAs<NullDevImpl>() );
unsigned toget = args->nbytes_;
#ifdef NULLDEV_DEBUG
fprintf(CPSW::fDbg(), "off %llu, dbytes %u, size %lu\n", (unsigned long long)args->off_, args->nbytes_, owner->getSize());
#endif
	if ( owner->getSize() > 0 && args->off_ + toget > owner->getSize() ) {
		throw ConfigurationError("NullAddress: read out of range");
	}

	::memset(args->dst_, 0, toget);

#ifdef NULLDEV_DEBUG
fprintf(CPSW::fDbg(), "NullDev read from off %lli to %p:", (unsigned long long)args->off_, args->dst_);
for ( unsigned ii=0; ii<args->nbytes_; ii++)
	fprintf(CPSW::fDbg(), " 0x%02x", args->dst_[ii]);
fprintf(CPSW::fDbg(), "\n");
#endif

	return toget;
}

uint64_t CNullAddressImpl::write(CWriteArgs *args) const
{
NullDevImpl owner( getOwnerAs<NullDevImpl>() );
unsigned put  = args->nbytes_;
unsigned rval = put;

	if ( owner->getSize() &&  args->off_ + put > owner->getSize() ) {
		throw ConfigurationError("NullAddress: write out of range");
	}

#ifdef NULLDEV_DEBUG
fprintf(CPSW::fDbg(), "NullDev write to off %lli from %p:", (unsigned long long)args->off_, args->src_);
for ( unsigned ii=0; ii<args->nbytes_; ii++)
	fprintf(CPSW::fDbg(), " 0x%02x", args->src_[ii]);
fprintf(CPSW::fDbg(), "\n");
#endif

	return rval;
}

NullDev INullDev::create(const char *name, uint64_t size)
{
	return CShObj::create<NullDevImpl>(name, size);
}
