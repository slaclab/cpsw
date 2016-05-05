#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_address.h>

using boost::dynamic_pointer_cast;

IEntryAdapt::IEntryAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
	:CShObj(k),
	 ie_(ie), p_(p->clone())
{
	if ( p->empty() )
		throw InvalidPathError("<EMPTY>");

	Address  a = CompositePathIterator( &p )->c_p_;

	if ( a->getEntryImpl() != ie )
		throw InternalError("inconsistent args passed to IEntryAdapt");
	if ( UNKNOWN == a->getByteOrder() ) {
		throw ConfigurationError("Configuration Error: byte-order not set");
	}
}


