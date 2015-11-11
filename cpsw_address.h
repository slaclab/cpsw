#ifndef CPSW_ADDRESS_H
#define CPSW_ADDRESS_H

#include <api_builder.h>
#include <stdio.h>
#include <cpsw_hub.h>

class AKey {
private:
	WContainer owner;
	AKey(Container owner):owner(owner) {}
public:
	const Container get() const { return Container(owner); }

	friend class DevImpl;
};

class AddressImpl : public IChild {
	protected:
		AKey           owner;
	private:
		Entry          child;
		unsigned       nelms;

	protected:
		ByteOrder      byteOrder;

	public:
		AddressImpl(AKey owner, unsigned nelms = 1, ByteOrder byteOrder = UNKNOWN);
		virtual ~AddressImpl()
		{
		}

		virtual void attach(Entry child)
		{
			if ( this->child != NULL ) {
				throw AddressAlreadyAttachedError( child->getName() );
			}
			this->child = child;
		}

		virtual Entry getEntry() const
		{
			return child;
		}

		virtual const char *getName() const
		{
			Entry e = getEntry();
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

		virtual uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;

		virtual void dump(FILE *f) const;

		virtual void dump() const
		{
			dump( stdout );
		}

		virtual Container getOwner() const;
};

#endif
