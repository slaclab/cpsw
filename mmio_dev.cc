#include <api_user.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <inttypes.h>
#include <stdio.h>

class MMIOAddress : public Address {
	private:
		uint64_t offset;    // in bytes
		uint64_t stride;    // in bytes

	protected:

		MMIOAddress(
			Dev *owner,
			uint64_t offset   = 0,
			unsigned nelms    = 1,
			uint64_t stride   = -1ULL );

		friend class MMIODev;

	public:
		virtual uint64_t getOffset() const   { return offset; }
		virtual uint64_t getStride() const   { return stride; }

		using Address::dump;

		virtual void dump(FILE *f) const;
		virtual uint64_t  read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual void attach(Entry *child);
};


class MMIODev : public Dev {
	public:
		MMIODev(const char *name, uint64_t size) : Dev(name, size)
		{
		}

		virtual void addAtAddr(Entry *child, uint64_t offset, unsigned nelms = 1, uint64_t stride = -1ULL) {

			if ( -1ULL == stride ) {
				stride = child->getSize();
			}


			if ( offset + (nelms-1) * stride + child->getSize() > getSize() ) {
				throw AddrOutOfRangeError(child->getName());
			}
			Address *a = new MMIOAddress(this, offset, nelms, stride);
			add(a, child);
		}
};

MMIOAddress::MMIOAddress(
			Dev *owner,
			uint64_t offset,
			unsigned nelms,
			uint64_t stride)
: offset(offset), stride(stride), Address(owner, nelms)
{
}

void MMIOAddress::dump(FILE *f) const
{
	Address::dump( f ); fprintf(f, "+0x%"PRIx64, offset);
}

uint64_t MMIOAddress::read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int        rval      = 0, fro, to;
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
