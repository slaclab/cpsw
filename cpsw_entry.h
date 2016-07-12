#ifndef CPSW_ENTRY_H
#define CPSW_ENTRY_H

#include <cpsw_api_builder.h>
#include <cpsw_shared_obj.h>

#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

namespace YAML {
	class Node;
};

using boost::weak_ptr;
using boost::make_shared;

class   Visitor;

class   CEntryImpl;
typedef shared_ptr<CEntryImpl> EntryImpl;

class CEntryImpl: public virtual IField, public CShObj {
	private:
		// WARNING -- when modifying fields you might need to
		//            modify 'operator=' as well as the copy
		//            constructor!
		std::string	        name_;
		std::string         description_;
	protected:
		uint64_t    	    size_;
	private:
		mutable Cacheable   cacheable_;
		mutable bool        locked_;


	private:

		void checkArgs();

	protected:
		// prevent public copy construction -- cannot copy the 'self'
		// member this constructor is intended be used by the 'clone'
		// template which takes care of setting 'self'.
		CEntryImpl(CEntryImpl &ei, Key &k);

	public:
		CEntryImpl(Key &k, const char *name, uint64_t size);

#ifdef WITH_YAML
		// every subclass must implement a constructor from a YAML::Node
		// which chains through the superclass constructor.
		CEntryImpl(Key &k, const YAML::Node &n);

		// every class implements this and MUST chain through its
		// superclass first (thus, derived classes can overwrite fields)
		// e.g.,
		//
		// void CEntryImplSubclass::dumpYamlPart(YAML::Node &node)
		// {
		//    CEntryImpl::dumpYamlPart( node ); // chain
		//
		//    dump subclass fields into node here (may override superclass entries)
		//    node["myField"] = myvalue;
		// }
		virtual void dumpYamlPart(YAML::Node &) const;

		// Every subclass MUST define a static className_ member and
		// assign it a unique value;
		static const char  *const className_;

		// Every subclass MUST implement the 'getClassName()' virtual
		// method. Just copy-paste this one:
		virtual const char *getClassName() const { return className_; }

		// subclass MAY implement 'overrideNode()' which is executed
		// by dispatchMakeField() before the new entity is created.
		// Can be used to modify the YAML node before it is passed
		// to the constructor (create a new node from 'orig', modify
		// and return it).
		static YAML::Node overrideNode(const YAML::Node &orig);

		// used by EntryImpl and DevImpl to append class name and iterate
		// through children; subclass probably wants to leave this alone
		virtual YAML::Node dumpYaml() const;
#endif

		virtual CEntryImpl *clone(Key &k) { return new CEntryImpl( *this, k ); }

		virtual const char *getName() const
		{
			return name_.c_str();
		}

		virtual const char *getDescription() const
		{
			return description_.c_str();
		}

		virtual void setDescription(const char *);
		virtual void setDescription(const std::string&);

		virtual uint64_t getSize() const
		{
			return size_;
		}

		virtual Cacheable getCacheable() const
		{
			return cacheable_;
		}

		// may throw exception if modified after
		// being attached
		virtual void setCacheable(Cacheable cacheable);

		virtual ~CEntryImpl();

		virtual void setLocked()
		{
			locked_ = true;
		}

		virtual bool getLocked()
		{
			return locked_;
		}

		virtual void accept(IVisitor    *v, RecursionOrder order, int recursionDepth);

		virtual EntryImpl getSelf()            { return getSelfAs<EntryImpl>();       }
		virtual EntryImpl getConstSelf() const { return getSelfAsConst<EntryImpl>(); }

		virtual Hub isHub() const   { return Hub( static_cast<Hub::element_type *>(NULL) ); }
};

#endif
