#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <cpsw_api_builder.h>
#include <map>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::weak_ptr;
using boost::make_shared;

class   Visitor;

class   CEntryImpl;
typedef shared_ptr<CEntryImpl> EntryImpl;
typedef weak_ptr<CEntryImpl>   WEntryImpl;

class   CDevImpl;
typedef shared_ptr<CDevImpl>    DevImpl;
typedef weak_ptr<CDevImpl>     WDevImpl;

// "Key" class to prevent the public from
// directly instantiating EntryImpl (and derived)
// objects since we want the public to 
// access EntryImpl via shared pointers only.
class FKey {
private:
	const char *name;
	FKey(const char *name):name(name){}
public:
	const char *getName() const { return name; }
	friend class CEntryImpl;
};

// debugging
extern int cpsw_obj_count;

class CEntryImpl: public virtual IField {
	private:
		// WARNING -- when modifying fields you might need to
		//            modify 'operator=' as well as the copy
		//            constructor!
		std::string	        name;
		std::string         description;
		uint64_t    	    size;
		mutable Cacheable   cacheable;
		mutable bool        locked;
		mutable WEntryImpl  self;

	protected:
		virtual void  setSelf(EntryImpl sp) { self = sp; }

	protected:
		// prevent public copy construction -- cannot copy the 'self'
		// member this constructor is intended be used by the 'clone'
		// template which takes care of setting 'self'.
		CEntryImpl(const CEntryImpl &ei);

	public:
		CEntryImpl(FKey k, uint64_t size);

		// need to override operator= to properly take care
		// of 'self'.
		CEntryImpl &operator=(const CEntryImpl &in);

		virtual const char *getName() const
		{
			return name.c_str();
		}

		virtual const char *getDescription() const
		{
			return description.c_str();
		}

		virtual void setDescription(const char *);
		virtual void setDescription(const std::string&);

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

		virtual ~CEntryImpl();

		virtual void setLocked()
		{
			locked = true;
		}

		virtual void accept(IVisitor    *v, RecursionOrder order, int recursionDepth);

		virtual EntryImpl getSelf() { return EntryImpl( self ); }

		// Template for up-casting derived classes' 'self' pointer
	protected:
		template <typename T> T getSelfAs()
		{
			return T( static_pointer_cast< typename T::element_type, CEntryImpl>( getSelf() ) );
		}

	public:
		// factory for derived types T
		template<typename T>
			static shared_ptr<T>
			create(const char *name)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name) ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1>
			static shared_ptr<T>
			create(const char *name, A1 a1)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name), a1 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name), a1, a2 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name), a1, a2, a3 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3, typename A4>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3, A4 a4)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name), a1, a2, a3, a4 ) );
				self->setSelf(self);
				return self;
			}
		template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5>
			static shared_ptr<T>
			create(const char *name, A1 a1, A2 a2, A3 a3, A4 a4, A5 a5)
			{
				shared_ptr<T> self( make_shared<T>( FKey(name), a1, a2, a3, a4, a5 ) );
				self->setSelf(self);
				return self;
			}

		template<typename T> static shared_ptr<T> clone(shared_ptr<T> in)
		{
			shared_ptr<T> self( make_shared<T>( *in ) );
			self->setSelf( self );
			return self;
		}
};

class CIntEntryImpl : public CEntryImpl, public virtual IIntField {
private:
	bool     is_signed;
	int      ls_bit;
	uint64_t size_bits;
	unsigned wordSwap;
public:
	CIntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit = 0, unsigned wordSwap = 0);

	virtual bool     isSigned()    const { return is_signed; }
	virtual int      getLsBit()    const { return ls_bit;    }
	virtual uint64_t getSizeBits() const { return size_bits; }
	virtual unsigned getWordSwap() const { return wordSwap;  }
};

struct StrCmp {
    bool operator () (const char *a, const char *b ) const {
        return strcmp(a , b) < 0 ? true : false;
    }
};

#include <cpsw_address.h>

class CDevImpl : public CEntryImpl, public virtual IDev {
	private:
		typedef  std::map<const char*, Address, StrCmp> Children;
		mutable  Children children; // only by 'add' method

	protected:
		virtual void add(AddressImpl a, Field child);

		virtual AKey getAKey()       { return AKey( getSelfAs<DevImpl>() );       }

	public:
		CDevImpl(FKey k, uint64_t size= 0);
		virtual ~CDevImpl();

		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddress(Field child, unsigned nelms)
		{
		    AKey k = getAKey();
			add( make_shared<CAddressImpl>(k, nelms), child->getSelf() );
		}

		virtual Path findByName(const char *s);

		virtual Child getChild(const char *name) const { return getAddress( name ); }
		virtual Address getAddress(const char *name) const;
	
		virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth);
};

#define NULLHUB     Hub( static_cast<IHub *>(NULL) )
#define NULLDEVIMPL DevImpl( static_cast<CDevImpl *>(NULL) )


#endif
