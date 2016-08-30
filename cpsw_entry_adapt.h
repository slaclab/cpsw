#ifndef CPSW_ENTRY_ADAPT_H
#define CPSW_ENTRY_ADAPT_H

#include <cpsw_api_builder.h>
#include <cpsw_entry.h>
#include <cpsw_address.h>
#include <cpsw_path.h>

using boost::dynamic_pointer_cast;

class IEntryAdapt : public virtual IEntry, public CShObj {
protected:
	shared_ptr<const CEntryImpl> ie_;
	ConstPath                    p_;


protected:
	IEntryAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie);

	// clone not implemented (should not be needed)

public:
	virtual const char *getName()        const { return ie_->getName(); }
	virtual const char *getDescription() const { return ie_->getDescription(); }
	virtual uint64_t    getSize()        const { return ie_->getSize(); }
	virtual Hub         isHub()          const { return ie_->isHub();   }
	virtual Path        getPath()        const { return p_->clone();    }

	template <typename ADAPT, typename IMPL> static ADAPT check_interface(Path p)
	{
		if ( p->empty() )
			throw InvalidArgError("Empty Path");

		Address a = CompositePathIterator( p )->c_p_;
		shared_ptr<const typename IMPL::element_type> e = dynamic_pointer_cast<const typename IMPL::element_type, CEntryImpl>( a->getEntryImpl() );
		if ( e ) {
			ADAPT rval = CShObj::template create<ADAPT>(p, e);
			return rval;
		}
		throw InterfaceNotImplementedError( p->toString() );
	}
};

#endif
