 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <cpsw_api_user.h>
#include <cpsw_stream_adapt.h>


CStreamAdapt::CStreamAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
: IEntryAdapt(k, p, ie)
{
	if ( p->getNelms() > 1 )
		throw ConfigurationError("CStreamAdapt -- arrays of streams not supported");
}


int64_t
CStreamAdapt::read(uint8_t *buf, uint64_t size, const CTimeout timeout, uint64_t off)
{
	CompositePathIterator it( p_);
	Address cl = it->c_p_;
	CReadArgs args;

	args.cacheable_ = ie_->getCacheable();
	args.dst_       = buf;
	args.nbytes_    = size;
	args.off_       = off;
	args.timeout_   = timeout;
	return cl->read( &it, &args );
}

int64_t
CStreamAdapt::write(uint8_t *buf, uint64_t size, const CTimeout timeout)
{
	CompositePathIterator it( p_);
	Address cl = it->c_p_;
	CWriteArgs args;

	args.cacheable_ = ie_->getCacheable();
	args.off_       = 0;
	args.src_       = buf;
	args.nbytes_    = size;
	args.msk1_      = 0;
	args.mskn_      = 0;
	args.timeout_   = timeout;
	return cl->write( &it, &args );
}

Stream IStream::create(Path p)
{
	return IEntryAdapt::check_interface<Stream>( p );
}
