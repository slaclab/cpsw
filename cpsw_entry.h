#ifndef CPSW_ENTRY_H
#define CPSW_ENTRY_H

#include <cpsw_api_builder.h>

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::weak_ptr;
using boost::make_shared;

class   Visitor;

class   CEntryImpl;
typedef shared_ptr<CEntryImpl> EntryImpl;
typedef weak_ptr<CEntryImpl>   WEntryImpl;

// debugging
extern int cpsw_obj_count;

class CEntryImpl: public virtual IField {
	public:
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
		CEntryImpl(CEntryImpl &ei);

	public:
		CEntryImpl(FKey k, uint64_t size);

		// need to override operator= to properly take care
		// of 'self'.
		CEntryImpl &operator=(CEntryImpl &in);

		virtual CEntryImpl *clone(FKey k) { return new CEntryImpl( *this ); }

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

		virtual EntryImpl getSelf()            { return EntryImpl( self ); }
		virtual EntryImpl getConstSelf() const { return EntryImpl( self ); }

		virtual Hub isHub() const   { return Hub( static_cast<Hub::element_type *>(NULL) ); }

		// Template for up-casting derived classes' 'self' pointer
	protected:
		template <typename T> T getSelfAs() const
		{
			return T( static_pointer_cast< typename T::element_type, CEntryImpl>( getConstSelf() ) );
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
			typename T::element_type *p = in->clone(FKey(0));
			if ( typeid( *p ) != typeid( *in ) ) {
				delete( p );
				throw InternalError("Some subclass of EntryImpl doesn't implement 'clone'");
			}
			shared_ptr<T> self( p );
			self->setSelf( self );
			return self;
		}
};

#endif
