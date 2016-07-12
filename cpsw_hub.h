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
	private:
		typedef  std::map<const char*, Address, StrCmp> MyChildren;
		mutable  MyChildren children_; // only by 'add' method

	protected:
		virtual void add(AddressImpl a, Field child);

		virtual IAddress::AKey getAKey()  { return IAddress::AKey( getSelfAs<DevImpl>() );       }

		CDevImpl(CDevImpl &orig, Key &k):CEntryImpl(orig, k) { throw InternalError("Clone NotImplemented"); }

	public:
		CDevImpl(Key &k, const char *name, uint64_t size= 0);

#ifdef WITH_YAML
		CDevImpl(Key &k, const YAML::Node &n);

		virtual void dumpYamlPart(YAML::Node &node) const;

		static  const char *_getClassName()       { return "Dev";           }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual YAML::Node dumpYaml() const;
#endif
		virtual ~CDevImpl();

		virtual CDevImpl *clone(Key &k) { return new CDevImpl( *this, k ); }

		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddress(Field child, unsigned nelms)
		{
		    IAddress::AKey k = getAKey();
			add( make_shared<CAddressImpl>(k, nelms), child->getSelf() );
		}

#ifdef WITH_YAML
		virtual void addAtAddress(Field child, const YAML::Node &n);
#endif

		virtual Path findByName(const char *s);

		virtual Child getChild(const char *name) const { return getAddress( name ); }
		virtual Address getAddress(const char *name) const;
	
		virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth);

		virtual Children getChildren() const;

		virtual Hub isHub() const;

};

#define NULLHUB     Hub( static_cast<IHub *>(NULL) )
#define NULLDEVIMPL DevImpl( static_cast<CDevImpl *>(NULL) )


#endif
