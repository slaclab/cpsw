#ifndef CPSW_ADDRESS_H
#define CPSW_ADDRESS_H

#include <api_user.h>
#include <stdio.h>

class Dev;

class Address : public Child {
	private:
		const Dev*     owner;
		const Entry*   child;
		unsigned       nelms;

	protected:
		Address(Dev *owner, unsigned nelms = 1):owner(owner), nelms(nelms) {}

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

		virtual uint64_t read(CompositePathIterator *node, uint8_t *dst, uint64_t off, int headBits, uint64_t sizeBits) const;

		virtual void dump(FILE *f) const;

		virtual void dump() const
		{
			dump( stdout );
		}
};

#endif
