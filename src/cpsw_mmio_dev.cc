 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_mmio_dev.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cpsw_yaml.h>
#include <cpsw_async_io.h>
#include <cpsw_stdio.h>

//#define MMIODEV_DEBUG

CMMIOAddressImpl::CMMIOAddressImpl(
			AKey      key,
			uint64_t  offset,
			unsigned  nelms,
			uint64_t  stride,
			ByteOrder byteOrder)
: CAddressImpl(key,
              nelms,
              UNKNOWN == byteOrder ? key.getAs<MMIODevImpl>()->getByteOrder() : byteOrder ),
              offset_(offset),
              stride_(stride)
{
}

CMMIOAddressImpl::CMMIOAddressImpl(AKey key, YamlState &ypath)
: CAddressImpl( key, ypath            ),
  stride_     ( IMMIODev::DFLT_STRIDE )
{
	if ( readNode(ypath, YAML_KEY_offset, &offset_ ) ) {
		readNode(ypath, YAML_KEY_stride,    &stride_);
	} else {
		throw ConfigurationError("CMMIOAddressImpl: missing '" YAML_KEY_offset "' in YAML");
	}
}

void CMMIOAddressImpl::dump(FILE *f) const
{
	CAddressImpl::dump( f ); fprintf(f, "+0x%" PRIx64 " (stride %" PRId64 ")", offset_, stride_);
}

uint64_t CMMIOAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
int        rval      = 0, to;
uintptr_t  dstStride = node->getNelmsRight() * args->nbytes_;
IndexRange singleElement(0); // IndexRange operates 'inside' the array bounds defined by the path
IndexRange *range_p;

#ifdef MMIODEV_DEBUG
	fprintf(CPSW::fDbg(), "MMIO read; nelmsRight: %lld, nbytes_ %lld, stride %lld, idxf %lld, idxt %lld\n",
		(unsigned long long)node->getNelmsRight(),
		(unsigned long long)args->nbytes_,
		(unsigned long long)getStride(),
		(unsigned long long)(*node)->idxf_,
		(unsigned long long)(*node)->idxt_);
#endif

CReadArgs  nargs = *args;

	if ( nargs.nbytes_ == getStride()  && getEntryImpl()->getCacheable() >= IField::WT_CACHEABLE ) {
		// if strides == size then we can try to read all in one chunk
		nargs.nbytes_ *= (*node)->idxt_ - (*node)->idxf_ + 1;
		to      = (*node)->idxf_;
		range_p = &singleElement;
	} else {
		to      = (*node)->idxt_;
		range_p = 0;
	}

	nargs.off_ += this->offset_ + (*node)->idxf_ * getStride();

	/* If we break the read into multiple operations and this is an asynchronous operation
	 * then we must only notify our caller whan all the operations are complete.
	 * We use the 'AsyncIOParallelCompletion' to hold the original aio; when the last
	 * shared pointer to the parallel context goes out of scope (= when all the
	 * parallel read operations are complete) then we can conveniently execute
	 * the original callback from the parallel context's destructor.
	 */
	if ( to > (*node)->idxf_ && nargs.aio_ ) {
		nargs.aio_ = IAsyncIOParallelCompletion::create( nargs.aio_ );
	}

	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		SlicedPathIterator it( *node, range_p );
		rval += dispatchRead(&it, &nargs);

		nargs.off_ += getStride();

		nargs.dst_ += dstStride;
	}

	return rval;
}

uint64_t CMMIOAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
int        rval      = 0, to;
uintptr_t  srcStride = node->getNelmsRight() * args->nbytes_;
IndexRange singleElement(0); // IndexRange operates 'inside' the array bounds defined by the path
IndexRange *range_p;

#ifdef MMIODEV_DEBUG
	fprintf(CPSW::fDbg(), "MMIO write; nelmsRight: %d, nbytes_ %d\n", node->getNelmsRight(), args->nbytes_);
#endif

CWriteArgs nargs = *args;

	if (    nargs.nbytes_ == getStride()
	     && getEntryImpl()->getCacheable() >= IField::WB_CACHEABLE
	     && nargs.msk1_ == 0
	     && nargs.mskn_ == 0
	   ) {
		// if strides == size and we don't have to merge bits
		// then we can try to write all in one chunk
		nargs.nbytes_ *= (*node)->idxt_ - (*node)->idxf_ + 1;
		to             = (*node)->idxf_;
		range_p        = &singleElement;
#ifdef MMIODEV_DEBUG
		fprintf(CPSW::fDbg(), "MMIO write; collapsing range\n");
#endif
	} else {
		to             = (*node)->idxt_;
		range_p        = 0;
#ifdef MMIODEV_DEBUG
		fprintf(CPSW::fDbg(), "MMIO write; NOT collapsing range\n");
#endif
	}
	nargs.off_ += this->offset_ + (*node)->idxf_ * getStride();

#ifdef MMIODEV_DEBUG
	fprintf(CPSW::fDbg(), "MMIO write; iterating from %d -> %d\n", (*node)->idxf_, to);
#endif
	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		SlicedPathIterator it( *node, range_p );
		rval += dispatchWrite(&it, &nargs);
	
		nargs.off_ += getStride();

		nargs.src_ += srcStride;
	}

	return rval;
}

void CMMIOAddressImpl::attach(EntryImpl child)
{
	if ( IMMIODev::STRIDE_AUTO == stride_ ) {
		stride_ = child->getSize();
	}

	if ( getOffset() + (getNelms()-1) * getStride() + child->getSize() > getOwner()->getSize() ) {
		throw AddrOutOfRangeError(child->getName());
	}

	CAddressImpl::attach(child);
}

MMIODev IMMIODev::create(const char *name, uint64_t size, ByteOrder byteOrder)
{
	return CShObj::create<MMIODevImpl>(name, size, byteOrder);
}

void CMMIODevImpl::addAtAddress(Field child, uint64_t offset, unsigned nelms, uint64_t stride, ByteOrder byteOrder)
{
	IAddress::AKey k = getAKey();
	add(
			cpsw::make_shared<CMMIOAddressImpl>(k, offset, nelms, stride, byteOrder),
			child
	   );
}

void
CMMIODevImpl::addAtAddress(Field child, YamlState &ypath)
{
uint64_t  offset;
	if ( readNode(ypath, YAML_KEY_offset, &offset ) ) {
		doAddAtAddress<CMMIOAddressImpl>( child, ypath );
	} else {
		fprintf(CPSW::fErr(), "WARNING: no '" YAML_KEY_offset "' attribute found in YAML -- adding '%s' as an non-MMIO/unknown device\n", child->getName());
		doAddAtAddress<CAddressImpl>( child, ypath );
	}
}

void
CMMIOAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	CAddressImpl::dumpYamlPart( node );
	writeNode(node, YAML_KEY_offset, offset_);
	writeNode(node, YAML_KEY_stride, stride_);
}

CMMIODevImpl::CMMIODevImpl(Key &key, YamlState &ypath)
: CDevImpl(key, ypath),
  byteOrder_(DFLT_BYTE_ORDER)
{
	if ( 0 == size_ ) {
		throw InvalidArgError("'size' zero or unset");
	}

	readNode( ypath, YAML_KEY_byteOrder, &byteOrder_ );
}

void
CMMIODevImpl::dumpYamlPart(YAML::Node &node) const
{
	CDevImpl::dumpYamlPart(node);
	writeNode(node, YAML_KEY_byteOrder, byteOrder_);
}
