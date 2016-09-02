 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_address.h>

using boost::dynamic_pointer_cast;

IEntryAdapt::IEntryAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
	:CShObj(k),
	 ie_(ie), p_(p->clone())
{
	if ( p->empty() )
		throw InvalidPathError("<EMPTY>");

	CompositePathIterator it( p );

	Address  a = it->c_p_;

	if ( a->getEntryImpl() != ie )
		throw InternalError("inconsistent args passed to IEntryAdapt");
	if ( UNKNOWN == a->getByteOrder() ) {
		throw ConfigurationError("Configuration Error: byte-order not set");
	}

	a->open( &it );
}

void
IEntryAdapt::setUnique(CEntryImpl::UniqueHandle uniqueHandle)
{
	uniq_ = uniqueHandle;
}

IEntryAdapt::~IEntryAdapt()
{
	CompositePathIterator it( p_ );

	Address  a = it->c_p_;

	a->close( &it );
}
