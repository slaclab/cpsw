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

void CMMIOAddressImpl::dump(FILE *f) const
{
	CAddressImpl::dump( f ); fprintf(f, "+0x%"PRIx64" (stride %"PRId64")", offset_, stride_);
}


class CParallelReadCtxt;
typedef shared_ptr<CParallelReadCtxt> ParallelReadCtxt;

class CParallelReadCtxt : public IAsyncIO {
private:
	AsyncIO      aio_;
	CPSWErrorHdl err_;
	CMtx         mtx_;
public:
	CParallelReadCtxt(AsyncIO aio)
	: aio_(aio)
	{
#ifdef MMIODEV_DEBUG
printf("Parallel Ctxt CREAT\n");
#endif
	}

	virtual void callback(CPSWError *err)
	{
		/* No real work to do; simply record the first error (in a thread safe way) */
		if ( err && ! err_ ) {
			CMtx::lg GUARD( &mtx_ );

			if ( ! err_ ) {
				err_ = err->clone();
			}
		}
	}

	// Use the shared pointer which references this object
	// as a reference-counter.
	virtual ~CParallelReadCtxt()
	{
#ifdef MMIODEV_DEBUG
printf("Parallel Ctxt DEST\n");
#endif
		aio_->callback( err_ ? err_.get() : 0 );
	}

	static ParallelReadCtxt create(AsyncIO aio)
	{
		return make_shared<CParallelReadCtxt>(aio);
	}
};

uint64_t CMMIOAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
int        rval      = 0, to;
uintptr_t  dstStride = node->getNelmsRight() * args->nbytes_;
IndexRange singleElement(0); // IndexRange operates 'inside' the array bounds defined by the path
IndexRange *range_p;

#ifdef MMIODEV_DEBUG
	printf("MMIO read; nelmsRight: %d, nbytes_ %d, stride %d, idxf %d, idxt %d\n", node->getNelmsRight(), args->nbytes_, getStride(), (*node)->idxf_, (*node)->idxt_);
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
	 * We use the 'ParallelReadCtxt' to hold the original aio; when the last
	 * shared pointer to the parallel context goes out of scope (= when all the
	 * parallel read operations are complete) then we can conveniently execute
	 * the original callback from the parallel context's destructor.
	 */
	if ( to > (*node)->idxf_ && nargs.aio_ ) {
		nargs.aio_ = CParallelReadCtxt::create( nargs.aio_ );
	}

	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		SlicedPathIterator it( *node, range_p );
		rval += CAddressImpl::read(&it, &nargs);

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
	printf("MMIO write; nelmsRight: %d, nbytes_ %d\n", node->getNelmsRight(), args->nbytes_);
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
		printf("MMIO write; collapsing range\n");
#endif
	} else {
		to             = (*node)->idxt_;
		range_p        = 0;
#ifdef MMIODEV_DEBUG
		printf("MMIO write; NOT collapsing range\n");
#endif
	}
	nargs.off_ += this->offset_ + (*node)->idxf_ * getStride();

#ifdef MMIODEV_DEBUG
	printf("MMIO write; iterating from %d -> %d\n", (*node)->idxf_, to);
#endif
	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		SlicedPathIterator it( *node, range_p );
		rval += CAddressImpl::write(&it, &nargs);
	
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
			make_shared<CMMIOAddressImpl>(k, offset, nelms, stride, byteOrder),
			child
	   );
}

void
CMMIODevImpl::addAtAddress(Field child, YamlState &ypath)
{
uint64_t  offset;
unsigned  nelms     = DFLT_NELMS;
uint64_t  stride    = DFLT_STRIDE;
ByteOrder byteOrder = DFLT_BYTE_ORDER;

	if ( readNode(ypath, YAML_KEY_offset, &offset ) ) {
		readNode    (ypath, YAML_KEY_nelms,     &nelms);
		readNode    (ypath, YAML_KEY_stride,    &stride);
		readNode    (ypath, YAML_KEY_byteOrder, &byteOrder);

		addAtAddress(child, offset, nelms, stride, byteOrder);
	} else {
		CDevImpl::addAtAddress(child, ypath);
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
