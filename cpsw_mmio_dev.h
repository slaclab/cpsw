#ifndef CPSW_MMIO_DEV_H
#define CPSW_MMIO_DEV_H

#include <api_user.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <stdint.h>
#include <stdio.h>

class MMIODev;

class MMIOAddress : public Address {
	private:
		uint64_t offset;    // in bytes
		uint64_t stride;    // in bytes

	protected:
		MMIODev *owner;

		MMIOAddress(
			MMIODev *owner,
			uint64_t offset   = 0,
			unsigned nelms    = 1,
			uint64_t stride   = -1ULL,
			ByteOrder byteOrder = UNKNOWN );

		friend class MMIODev;

	public:
		virtual uint64_t getOffset() const   { return offset; }
		virtual uint64_t getStride() const   { return stride; }

		virtual int       getLdWidth() const;

		using Address::dump;

		virtual void dump(FILE *f) const;
		virtual uint64_t  read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual void attach(Entry *child);
};


class MMIODev : public Dev {
	private:
		int       ldWidth;   // addressable unit
		ByteOrder byteOrder;

	public:
		MMIODev(const char *name, uint64_t size, int ldWidth, ByteOrder byteOrder) : Dev(name, size), ldWidth(ldWidth), byteOrder(byteOrder)
		{
		}

		virtual int      getLdWidth() const { return ldWidth; }

		virtual ByteOrder getByteOrder() const { return byteOrder; }

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

#endif
