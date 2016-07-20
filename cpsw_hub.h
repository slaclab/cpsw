#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <cpsw_api_builder.h>
#include <cpsw_address.h>
#include <cpsw_entry.h>

#include <map>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::weak_ptr;
using boost::make_shared;

class   Visitor;

class   CDevImpl;
typedef shared_ptr<CDevImpl>    DevImpl;
typedef weak_ptr<CDevImpl>     WDevImpl;

struct StrCmp {
    bool operator () (const char *a, const char *b ) const {
        return strcmp(a , b) < 0 ? true : false;
    }
};

class CDevImpl : public CEntryImpl, public virtual IDev {
	public:
		typedef  std::map<const char*, Address, StrCmp> MyChildren;

	private:
		mutable  MyChildren children_; // only by 'add' method

	protected:
		virtual void add(AddressImpl a, Field child);

		virtual IAddress::AKey getAKey()  { return IAddress::AKey( getSelfAs<DevImpl>() );       }

		CDevImpl(CDevImpl &orig, Key &k):CEntryImpl(orig, k) { throw InternalError("Clone NotImplemented"); }

	public:

		typedef MyChildren::const_iterator const_iterator;

		CDevImpl(Key &k, const char *name, uint64_t size= 0);

		CDevImpl(Key &k, const YAML::Node &n);

		virtual void dumpYamlPart(YAML::Node &node) const;

		static  const char *_getClassName()       { return "Dev";           }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual void dumpYaml(YAML::Node &node) const;
		virtual ~CDevImpl();

		virtual CDevImpl *clone(Key &k) { return new CDevImpl( *this, k ); }

		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddress(Field child, unsigned nelms)
		{
		    IAddress::AKey k = getAKey();
			add( make_shared<CAddressImpl>(k, nelms), child->getSelf() );
		}

		virtual void addAtAddress(Field child, const YAML::Node &n);

		virtual Path findByName(const char *s) const;

		virtual Child getChild(const char *name) const { return getAddress( name ); }
		virtual Address getAddress(const char *name) const;
	
		virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth);

		virtual Children getChildren() const;

		virtual Hub          isHub()          const;
		virtual ConstDevImpl isConstDevImpl() const  { return getSelfAsConst<ConstDevImpl>(); }

		virtual const_iterator begin() const
		{
			return children_.begin();
		}

		virtual const_iterator end()   const
		{
			return children_.end();
		}

};

#define NULLHUB     Hub( static_cast<IHub *>(NULL) )
#define NULLDEVIMPL DevImpl( static_cast<CDevImpl *>(NULL) )


#endif
