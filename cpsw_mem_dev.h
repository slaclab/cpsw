#ifndef CPSW_MEM_TST_DEV_H
#define CPSW_MEM_TST_DEV_H

#include <api_user.h>
#include <cpsw_hub.h>

// pseudo-device with memory backing (for testing)

class MemDev;

class MemDevAddress : public Address {
	protected:
		MemDevAddress(MemDev *owner, unsigned nelms = 1);

		friend class MemDev;

	public:
		virtual uint64_t read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
};

class MemDev : public Dev {
	public:
		uint8_t *buf;

		MemDev(const char *name, uint64_t size);

		virtual ~MemDev();

		virtual void addAtAddr(Entry *child, unsigned nelms = 1);
};

#endif
