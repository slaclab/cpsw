#ifndef CPSW_MEM_TST_DEV_H
#define CPSW_MEM_TST_DEV_H

#include <cpsw_hub.h>

class CMemDevImpl;
typedef shared_ptr<CMemDevImpl> MemDevImpl;

// pseudo-device with memory backing (for testing)

class CMemAddressImpl : public CAddressImpl {
	public:
		CMemAddressImpl(AKey key, unsigned nelms = 1);

		// ANY subclass must implement clone(AKey) !
		virtual CMemAddressImpl *clone(AKey k) { return new CMemAddressImpl( *this ); }

		virtual uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;
};

class CMemDevImpl : public CDevImpl, public virtual IMemDev {
	private:
		uint8_t * buf_;
	protected:
		CMemDevImpl(CMemDevImpl &orig);

	public:
		CMemDevImpl(FKey k, uint64_t size);

		CMemDevImpl & operator=(CMemDevImpl &orig);

		virtual void addAtAddress(Field child, unsigned nelms = 1);

		virtual uint8_t * const getBufp() const { return buf_; }

		virtual CMemDevImpl *clone(FKey k) { return new CMemDevImpl( *this ); }

		virtual ~CMemDevImpl();
};

#endif
