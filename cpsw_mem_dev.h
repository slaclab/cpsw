#ifndef CPSW_MEM_TST_DEV_H
#define CPSW_MEM_TST_DEV_H

#include <cpsw_hub.h>

class CMemDevImpl;
typedef shared_ptr<CMemDevImpl> MemDevImpl;

// pseudo-device with memory backing (for testing)

class CMemDevAddressImpl : public CAddressImpl {
	public:
		CMemDevAddressImpl(AKey key, unsigned nelms = 1);

		virtual uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;
};

class CMemDevImpl : public CDevImpl, public virtual IMemDev {
	private:
		uint8_t * const buf;
	public:
		CMemDevImpl(FKey k, uint64_t size);

		virtual void addAtAddress(Field child, unsigned nelms = 1);

		virtual uint8_t * const getBufp() const { return buf; }

		virtual ~CMemDevImpl();
};

#endif
