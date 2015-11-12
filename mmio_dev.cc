#include <cpsw_mmio_dev.h>
#include <inttypes.h>

MMIOAddressImpl::MMIOAddressImpl(
			AKey     key,
			uint64_t offset,
			unsigned nelms,
			uint64_t stride,
			ByteOrder byteOrder)
: AddressImpl(key,
              nelms,
              UNKNOWN == byteOrder ? static_pointer_cast<MMIODevImpl, DevImpl>(key.get())->getByteOrder() : byteOrder ),
              offset(offset),
              stride(stride)
{
}

/* old RHEL compiler */
#ifndef PRIx64
#define PRIx64 "lx"
#endif

void MMIOAddressImpl::dump(FILE *f) const
{
	AddressImpl::dump( f ); fprintf(f, "+0x%"PRIx64, offset);
}

uint64_t MMIOAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int        rval      = 0, to;
uintptr_t  dstStride = node->getNelmsRight() * dbytes;

	if ( sbytes == getStride()  && dbytes == sbytes && getEntry()->getCacheable() >= IField::WT_CACHEABLE ) {
		// if strides == size then we can try to read all in one chunk
		sbytes *= (*node)->idxt - (*node)->idxf + 1;
		dbytes  = sbytes;
		to      = (*node)->idxf;
	} else {
		to      = (*node)->idxt;
	}
	for ( int i = (*node)->idxf; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += AddressImpl::read(&it, cacheable, dst, dbytes, off + this->offset + stride *i, sbytes);

		dst  += dstStride;
	}

	return rval;
}

uint64_t MMIOAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
int        rval      = 0, to;
uintptr_t  srcStride = node->getNelmsRight() * sbytes;

	if ( dbytes == getStride()  && sbytes == dbytes && getEntry()->getCacheable() >= IField::WT_CACHEABLE ) {
		// if strides == size then we can try to read all in one chunk
		dbytes *= (*node)->idxt - (*node)->idxf + 1;
		sbytes  = dbytes;
		to      = (*node)->idxf;
	} else {
		to      = (*node)->idxt;
	}
	for ( int i = (*node)->idxf; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += AddressImpl::write(&it, cacheable, src, sbytes, off + this->offset + stride *i, dbytes, msk1, mskn);

		src  += srcStride;
	}

	return rval;
}

void MMIOAddressImpl::attach(Entry child)
{
	if ( IMMIODev::STRIDE_AUTO == stride ) {
		stride = child->getSize();
	}

	if ( getOffset() + (getNelms()-1) * getStride() + child->getSize() > getOwner()->getSize() ) {
		throw AddrOutOfRangeError(child->getName());
	}

	AddressImpl::attach(child);
}

MMIODev IMMIODev::create(const char *name, uint64_t size, ByteOrder byteOrder)
{
	return EntryImpl::create<MMIODevImpl>(name, size, byteOrder);
}

void MMIODevImpl::addAtAddress(Field child, uint64_t offset, unsigned nelms, uint64_t stride, ByteOrder byteOrder)
{
	AKey k = getAKey();
	add(
			make_shared<MMIOAddressImpl>(k, offset, nelms, stride, byteOrder),
			child
	   );
}

