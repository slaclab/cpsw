#include <cpsw_mmio_dev.h>

MMIOAddress::MMIOAddress(
			MMIODev *owner,
			uint64_t offset,
			unsigned nelms,
			uint64_t stride,
			ByteOrder byteOrder)
: Address(owner, nelms, UNKNOWN == byteOrder ? owner->getByteOrder() : byteOrder), offset(offset), stride(stride), owner(owner)
{
/*
	if ( (owner->getLdWidth() - 1) & offset )
		throw InvalidArgError("Misaligned offset");
*/
}

int MMIOAddress::getLdWidth() const
{
	return owner->getLdWidth();
}

void MMIOAddress::dump(FILE *f) const
{
	Address::dump( f ); fprintf(f, "+0x%"PRIx64, offset);
}

uint64_t MMIOAddress::read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int        rval      = 0, to;
uintptr_t  dstStride = node->getNelmsRight() * dbytes;

	if ( sbytes == getStride()  && dbytes == sbytes && getEntry()->getCacheable() ) {
		// if strides == size then we can try to read all in one chunk
		sbytes *= (*node)->idxt - (*node)->idxf + 1;
		dbytes  = sbytes;
		to      = (*node)->idxf;
	} else {
		to      = (*node)->idxt;
	}
	for ( int i = (*node)->idxf; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += Address::read(&it, cacheable, dst, dbytes, off + this->offset + stride *i, sbytes);

		dst  += dstStride;
	}

	return rval;
}

void MMIOAddress::attach(Entry *child)
{
	if ( stride == -1ULL ) {
		stride = child->getSize();
	}
	Address::attach(child);
}
