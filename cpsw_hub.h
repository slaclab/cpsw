#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <api_builder.h>
#include <map>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::weak_ptr;
using boost::make_shared;

class   Visitor;
class   ImplVisitor;

class   EntryImpl;
typedef shared_ptr<EntryImpl> Entry;
typedef weak_ptr<EntryImpl>   WEntry;

class   DevImpl;
typedef shared_ptr<DevImpl>   Container;
typedef weak_ptr<DevImpl>     WContainer;

class FKey {
private:
	const char *name;
	FKey(const char *name):name(name){}
public:
	const char *getName() const { return name; }
	friend class EntryImpl;
};

class EntryImpl: public virtual IField {
	private:
		const std::string	name;
		const uint64_t    	size;
		mutable Cacheable   cacheable;
		mutable bool        locked;
		WEntry              self;

	protected:
		virtual void  setSelf(Entry sp) { self = sp; }

	public:
		EntryImpl(FKey k, uint64_t size);

		virtual const char *getName() const
		{
			return name.c_str();
		}

		virtual uint64_t getSize() const
		{
			return size;
		}

		virtual Cacheable getCacheable() const
		{
			return cacheable;
		}

		// may throw exception if modified after
		// being attached
		virtual void setCacheable(Cacheable cacheable);

		virtual ~EntryImpl()
		{
			//printf("Deleting %s\n", getName());			
		}

		virtual void setLocked()
		{
			locked = true;
		}

		virtual void accept(IVisitor    *v, RecursionOrder order, int recursionDepth);

		virtual Entry getSelf() { return Entry( self ); }

		template <typename T> T getSelfAs()
		{
			return T( static_pointer_cast< typename T::element_type, EntryImpl>( getSelf() ) );
		}

		// factory for derived types T
		template<typename T>
			static shared_ptr<T>
			create(const char *name)
			{
				shared_ptr<T> self( new T( FKey(name) ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1>
			static shared_ptr<T>
			create(const char *name, A1 a1)
			{
				shared_ptr<T> self( new T( FKey(name), a1 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2)
			{
				shared_ptr<T> self( new T( FKey(name), a1, a2 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3)
			{
				shared_ptr<T> self( new T( FKey(name), a1, a2, a3 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3, typename A4>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3, A4 a4)
			{
				shared_ptr<T> self( new T( FKey(name), a1, a2, a3, a4 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
			{
				shared_ptr<T> self( new T( FKey(name), a1, a2, a3, a4, a5 ) );
				self->setSelf(self);
				return self;
			}
};

class IntEntryImpl : public EntryImpl, public virtual IIntField {
private:
	bool     is_signed;
	int      ls_bit;
	uint64_t size_bits;
	int      wordSwap;
public:
	IntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit = 0, int wordSwap = 0);

	virtual bool     isSigned()    const { return is_signed; }
	virtual int      getLsBit()    const { return ls_bit;    }
	virtual uint64_t getSizeBits() const { return size_bits; }
	virtual int      getWordSwap() const { return wordSwap;  }
};

class IChild {
	public:
		virtual       Container getOwner()     const = 0;
		virtual const char      *getName()     const = 0;
		virtual       Entry     getEntry()     const = 0;
		virtual       unsigned  getNelms()     const = 0;
		virtual ByteOrder   getByteOrder()     const = 0;
		virtual uint64_t       read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const = 0;
		virtual ~IChild() {}
};

#define NULLCHILD Child( static_cast<IChild *>(NULL) )
#define NULLHUB   Hub( static_cast<IHub *>(NULL) )

#include <cpsw_address.h>

struct StrCmp {
    bool operator () (const char *a, const char *b ) const {
        return strcmp(a , b) < 0 ? true : false;
    }
};

class DevImpl : public EntryImpl, public virtual IDev {
	private:
		typedef  std::map<const char*, shared_ptr<AddressImpl>, StrCmp> Children;
		mutable  Children children; // only by 'add' method

	protected:
		virtual void add(shared_ptr<AddressImpl> a, Field child);

		virtual AKey getAKey()       { return AKey( getSelfAs<Container>() );       }

	public:
		DevImpl(FKey k, uint64_t size= 0);
		virtual ~DevImpl();

		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddress(Field child, unsigned nelms)
		{
		    AKey k = getAKey();
			add( make_shared<AddressImpl>(k, nelms), child->getSelf() );
		}

		virtual Path findByName(const char *s);

		virtual Child getChild(const char *name) const;
	
		virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth);
};


#endif
