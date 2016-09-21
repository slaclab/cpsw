 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_ENTRY_ADAPT_H
#define CPSW_ENTRY_ADAPT_H

#include <cpsw_api_builder.h>
#include <cpsw_entry.h>
#include <cpsw_address.h>
#include <cpsw_path.h>

using boost::dynamic_pointer_cast;

class IEntryAdapt;
typedef shared_ptr<IEntryAdapt> EntryAdapt;

class IEntryAdapt : public virtual IVal_Base, public CShObj {
protected:
	shared_ptr<const CEntryImpl> ie_;
	ConstPath                    p_;
	CEntryImpl::UniqueHandle     uniq_;


protected:
	IEntryAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie);

	// initialize after object is created
    virtual void        postCreateHook() {}

	// clone not implemented (should not be needed)

private:
	virtual void        setUnique(CEntryImpl::UniqueHandle);

public:
	virtual const char *getName()        const { return ie_->getName();          }
	virtual const char *getDescription() const { return ie_->getDescription();   }
	virtual uint64_t    getSize()        const { return ie_->getSize();          }
	virtual Hub         isHub()          const { return ie_->isHub();            }
	virtual Path        getPath()        const { return p_->clone();             }
	virtual ConstPath   getConstPath()   const { return p_;                      }
	virtual unsigned    getNelms()             { return p_->getNelms();          }

	virtual            ~IEntryAdapt();

    // If a subclass wishes to prohibit an interface to be created more
    // than once for a given path (= device instance array) then it should
    // override 'singleInterfaceOnly()' and return 'true'.
	// You want singleInterfaceOnly for adapters which hold absolute state
	// information, for example cached values.
    static  bool        singleInterfaceOnly()  { return false;          }

	// The new version of 'check_interface' is called by the interface 'factory'/create
	// static member function. It uses polymorphism to let the implementation create
	// a suitable 'Adapter' so that there can be multiple implementations of the same
	// interface (CEntryImpl::createAdapter()).
	template <typename INTERF> static INTERF check_interface(Path p);

	// This is the 'legacy' version of 'check_interface'. Left here during  migration phase
	template <typename ADAPT, typename IMPL> static ADAPT check_interface(Path p);
};


class IEntryAdapterKey
{
	private:
		IEntryAdapterKey() {}
		IEntryAdapterKey(const IEntryAdapterKey&);
		IEntryAdapterKey &operator=(const IEntryAdapterKey&);

		template <typename I> friend I IEntryAdapt::check_interface(Path p);
		template <typename A, typename I> friend A IEntryAdapt::check_interface(Path p);
};

template <typename INTERF> INTERF IEntryAdapt::check_interface(Path p)
{
	if ( p->empty() )
		throw InvalidArgError("Empty Path");

	Address   a ( CompositePathIterator( p )->c_p_ );
    EntryImpl ei( a->getEntryImpl() );

	IEntryAdapterKey key;

	EntryAdapt adapter( ei->createAdapter( key, p, typeid(typename INTERF::element_type) ) );
	INTERF     rval;
	if ( adapter && (rval = dynamic_pointer_cast<typename INTERF::element_type>( adapter )) ) {
		CEntryImpl::UniqueHandle uniq;

		if ( singleInterfaceOnly() ) {
			// getUniqueHandle throws "MultipleInstantiationError" if  Path 'p'
			// overlaps with the path of an existing interface to Entry 'e'
			uniq = ei->getUniqueHandle( key, p );
		}

		if ( uniq )
			adapter->setUnique( uniq );

		adapter->postCreateHook();

		return rval;
	}
	throw InterfaceNotImplementedError( p->toString() );
}

template <typename ADAPT, typename IMPL> ADAPT IEntryAdapt::check_interface(Path p)
{
	if ( p->empty() )
		throw InvalidArgError("Empty Path");

	Address a = CompositePathIterator( p )->c_p_;
	shared_ptr<const typename IMPL::element_type> e = dynamic_pointer_cast<const typename IMPL::element_type, CEntryImpl>( a->getEntryImpl() );
	if ( e ) {
		CEntryImpl::UniqueHandle uniq;

		if ( singleInterfaceOnly() ) {
			IEntryAdapterKey key;
			// getUniqueHandle throws "MultipleInstantiationError" if  Path 'p'
			// overlaps with the path of an existing interface to Entry 'e'
			uniq = e->getUniqueHandle( key, p );
		}

		ADAPT rval = CShObj::template create<ADAPT>(p, e);

		if ( uniq )
			rval->setUnique( uniq );

		return rval;
	}
	throw InterfaceNotImplementedError( p->toString() );
}


#endif
