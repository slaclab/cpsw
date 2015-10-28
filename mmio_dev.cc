#include <api_user.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <inttypes.h>
#include <stdio.h>

class MMIOAddress : public Address {
	private:
		uint64_t offset;    // in bytes
		uint64_t stride;    // in bytes
		int      headBits;  // bit-offset of the first bit in the first byte

	protected:

		MMIOAddress(
			Dev *owner,
			uint64_t offset   = 0,
			unsigned nelms    = 1,
			int      headBits = 0,
			uint64_t stride = 0 );

		friend class MMIODev;

	public:
		virtual uint64_t getOffset() const   { return offset; }
		virtual uint64_t getStride() const   { return stride; }
		virtual int      getHeadBits() const { return headBits; }

		using Address::dump;

		virtual void dump(FILE *f) const;
		virtual uint64_t  read(CompositePathIterator *node, uint8_t *dst, uint64_t off, int headBits, uint64_t sizeBits) const;
		virtual void attach(Entry *child);
};


class MMIODev : public Dev {
	public:
		MMIODev(const char *name, uint64_t sizeBits) : Dev(name, sizeBits)
		{
		}

		virtual void addAtAddr(Entry *child, uint64_t offset, unsigned nelms = 1, int headBits = 0, uint64_t stride = 0) {

			if ( 8*(offset + (nelms-1) * stride) + headBits + child->getSizeBits() > getSizeBits() ) {
				throw AddrOutOfRangeError(child->getName());
			}
			Address *a = new MMIOAddress(this, offset, nelms, headBits, stride);
			add(a, child);
		}
};

MMIOAddress::MMIOAddress(
			Dev *owner,
			uint64_t offset,
			unsigned nelms,
			int      headBits,
			uint64_t stride)
: offset(offset), stride(stride), headBits(headBits), Address(owner, nelms)
{
	if ( headBits < 0 || headBits > 7 )
			throw InvalidArgError("MMIOAddress: headBits must be in 0..7");
}

void MMIOAddress::dump(FILE *f) const
{
	Address::dump( f ); fprintf(f, "+0x%"PRIx64, offset);
}

uint64_t MMIOAddress::read(CompositePathIterator *node, uint8_t *dst, uint64_t off, int headBits, uint64_t sizeBits) const
{
int        rval = 0, fro, to;
uintptr_t  dstStride = node->getNelmsRight() * ((sizeBits+7)/8);

	headBits += this->headBits;
	if ( headBits > 7 ) {
		off++;
		headBits &= 7;
	}

	if ( sizeBits == 8*getStride()  && dstStride == getStride() ) {
		// if strides == size then we can try to read all in one chunk
		sizeBits *= (*node)->idxt - (*node)->idxf + 1;
		to        = (*node)->idxf;
	} else {
		to        = (*node)->idxt;
	}
	for ( int i = (*node)->idxf; i <= to; i++ ) {
		CompositePathIterator it = *node;
		rval += Address::read(&it, dst, off + this->offset + stride *i, headBits, sizeBits);
		dst  += dstStride;
	}
	return rval;
}

void MMIOAddress::attach(Entry *child)
{
	if ( stride == 0 ) {
		stride = (child->getSizeBits() + headBits + 7)/8;
	}
	Address::attach(child);
}
