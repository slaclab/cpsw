#ifndef CPSW_ADDRESS_H
#define CPSW_ADDRESS_H

#include <api_user.h>
#include <stdio.h>
#include <cpsw_hub.h>

class Dev;

class Address : public Child {
	protected:
		const Dev*     owner;
	private:
		const Entry*   child;
		unsigned       nelms;

	protected:
		ByteOrder      byteOrder;
		Address(Dev *owner, unsigned nelms = 1, ByteOrder byteOrder = UNKNOWN);

	// Only Dev and its subclasses may create addresses ...
	friend class Dev;

	public:
		virtual ~Address() {}

		virtual void attach(const Entry *child)
		{
			if ( this->child ) {
				throw AddressAlreadyAttachedError( getName() );
			}
			this->child = child;
		}

		virtual const Entry *getEntry() const
		{
			return child;
		}

		virtual const char *getName() const
		{
			const Entry *e = getEntry();
			return e ? e->getName() : NULL;
		}

		virtual unsigned getNelms() const
		{
			return nelms;
		}

		virtual ByteOrder getByteOrder() const
		{
			return byteOrder;
		}

		virtual uint64_t read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;

		virtual void dump(FILE *f) const;

		virtual void dump() const
		{
			dump( stdout );
		}

		virtual const IDev *getOwner() const;
};

#endif
