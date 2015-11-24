#include <cpsw_mmio_dev.h>
#include <inttypes.h>

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

/* old RHEL compiler */
#ifndef PRIx64
#define PRIx64 "lx"
#endif

void CMMIOAddressImpl::dump(FILE *f) const
{
	CAddressImpl::dump( f ); fprintf(f, "+0x%"PRIx64" (stride %"PRId64")", offset_, stride_);
}

uint64_t CMMIOAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int        rval      = 0, to;
uintptr_t  dstStride = node->getNelmsRight() * dbytes;

	if ( sbytes == getStride()  && dbytes == sbytes && getEntryImpl()->getCacheable() >= IField::WT_CACHEABLE ) {
		// if strides == size then we can try to read all in one chunk
		sbytes *= (*node)->idxt_ - (*node)->idxf_ + 1;
		dbytes  = sbytes;
		to      = (*node)->idxf_;
	} else {
		to      = (*node)->idxt_;
	}
	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += CAddressImpl::read(&it, cacheable, dst, dbytes, off + this->offset_ + stride_ *i, sbytes);

		dst  += dstStride;
	}

	return rval;
}

uint64_t CMMIOAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
int        rval      = 0, to;
uintptr_t  srcStride = node->getNelmsRight() * sbytes;

	if ( dbytes == getStride()  && sbytes == dbytes && getEntryImpl()->getCacheable() >= IField::WT_CACHEABLE ) {
		// if strides == size then we can try to read all in one chunk
		dbytes *= (*node)->idxt_ - (*node)->idxf_ + 1;
		sbytes  = dbytes;
		to      = (*node)->idxf_;
	} else {
		to      = (*node)->idxt_;
	}
	for ( int i = (*node)->idxf_; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += CAddressImpl::write(&it, cacheable, src, sbytes, off + this->offset_ + stride_ *i, dbytes, msk1, mskn);

		src  += srcStride;
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
	return CEntryImpl::create<CMMIODevImpl>(name, size, byteOrder);
}

void CMMIODevImpl::addAtAddress(Field child, uint64_t offset, unsigned nelms, uint64_t stride, ByteOrder byteOrder)
{
	IAddress::AKey k = getAKey();
	add(
			make_shared<CMMIOAddressImpl>(k, offset, nelms, stride, byteOrder),
			child
	   );
}

