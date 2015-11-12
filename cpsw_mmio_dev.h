#ifndef CPSW_MMIO_DEV_H
#define CPSW_MMIO_DEV_H

#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <stdint.h>
#include <stdio.h>

class MMIODevImpl;

class MMIOAddressImpl : public AddressImpl {
	private:
		uint64_t offset;    // in bytes
		uint64_t stride;    // in bytes

	public:
		MMIOAddressImpl(
			AKey     key,
			uint64_t offset   = 0,
			unsigned nelms    = 1,
			uint64_t stride   = IMMIODev::STRIDE_AUTO,
			ByteOrder byteOrder = UNKNOWN );

		virtual uint64_t getOffset() const   { return offset; }
		virtual uint64_t getStride() const   { return stride; }

		using AddressImpl::dump;

		virtual void dump(FILE *f) const;
		virtual uint64_t  read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;
		virtual void attach(Entry child);
};


class MMIODevImpl : public DevImpl, public virtual IMMIODev {
	private:
		ByteOrder byteOrder;

	public:
		MMIODevImpl(FKey k, uint64_t size, ByteOrder byteOrder) : DevImpl(k, size), byteOrder(byteOrder)
		{
		}

		virtual ByteOrder getByteOrder() const { return byteOrder; }

		virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO, ByteOrder byteOrder = UNKNOWN);

		using DevImpl::getName;
		virtual const char *getName() const { return DevImpl::getName(); }
};

#endif
