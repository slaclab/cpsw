#ifndef CPSW_ADDRESS_H
#define CPSW_ADDRESS_H

#include <cpsw_api_builder.h>
#include <stdio.h>
//#include <cpsw_hub.h>

#include <boost/weak_ptr.hpp>
using boost::weak_ptr;

class   CDevImpl;
class   CAddressImpl;
class   CompositePathIterator;
class   IAddress;

typedef shared_ptr<CDevImpl> DevImpl;
typedef weak_ptr<CDevImpl>   WDevImpl;
typedef shared_ptr<const IAddress> Address;
typedef shared_ptr<CAddressImpl> AddressImpl;


class IAddress : public IChild {
	public:
		class AKey {
			private:
				WDevImpl owner;
				AKey(DevImpl owner):owner(owner) {}
			public:
				const DevImpl get() const { return DevImpl(owner); }

				template <typename T> T getAs() const
				{
					return static_pointer_cast<typename T::element_type, DevImpl::element_type>( get() );
				}

				friend class CDevImpl;
		};

		virtual void attach(EntryImpl child) = 0;

		virtual uint64_t read (CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const = 0;
		virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const = 0;

		virtual void dump(FILE *f)           const = 0;

		virtual void dump()                  const = 0;

		virtual EntryImpl getEntryImpl()     const = 0;

		virtual DevImpl getOwnerAsDevImpl()  const = 0;

		virtual ByteOrder getByteOrder()     const = 0;

		virtual ~IAddress() {}
};

#define NULLCHILD Child( static_cast<IChild *>(NULL) )
#define NULLADDR  Address( static_cast<IAddress *>(NULL) )

class CAddressImpl : public IAddress {
	protected:
		mutable AKey   owner;
	private:
		mutable EntryImpl  child;
		unsigned       nelms;

	protected:
		ByteOrder      byteOrder;

	public:
		CAddressImpl(AKey owner, unsigned nelms = 1, ByteOrder byteOrder = UNKNOWN);
		virtual ~CAddressImpl()
		{
		}

		virtual void attach(EntryImpl child);

		virtual Entry getEntry() const
		{
			return child;
		}

		virtual EntryImpl getEntryImpl() const
		{
			return child;
		}

		virtual const char *getName() const;

		virtual const char *getDescription() const;
		virtual uint64_t    getSize() const;

		virtual unsigned getNelms() const
		{
			return nelms;
		}

		virtual ByteOrder getByteOrder() const
		{
			return byteOrder;
		}

		virtual uint64_t read (CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
		virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

		virtual void dump(FILE *f) const;

		virtual void dump() const
		{
			dump( stdout );
		}

		virtual Hub     getOwner()          const;
		virtual DevImpl getOwnerAsDevImpl() const;

	protected:
		template <typename T> T getOwnerAs() const
		{
			return static_pointer_cast<typename T::element_type, DevImpl::element_type>( this->getOwnerAsDevImpl() );
		}
};

#endif
