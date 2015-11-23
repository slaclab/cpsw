#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <ctype.h>

#include <stdint.h>

int cpsw_obj_count = 0;

CEntryImpl::CEntryImpl(FKey k, uint64_t size)
: name( k.getName() ),
  size( size),
  cacheable( UNKNOWN_CACHEABLE ),
  locked( false )
{
const char *cptr;
	for ( cptr = name.c_str(); *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(name);
	}
	cpsw_obj_count++;
}

CEntryImpl::CEntryImpl(CEntryImpl &ei)
: name(ei.name),
  description(ei.description),
  size(ei.size),
  cacheable(ei.cacheable),
  locked( false )
{
	self.reset();
}

CEntryImpl & CEntryImpl::operator=(CEntryImpl &in)
{
	if ( locked ) {
		throw ConfigurationError("Must not assign to attached EntryImpl");
	}
	if ( this != &in ) {
		name        = in.name;
		description = in.description;
		size        = in.size;
		cacheable   = in.cacheable;
		locked      = false;
		// do NOT copy 'self' but leave alone !!
	}
	return *this;
}

CEntryImpl::~CEntryImpl()
{
	cpsw_obj_count--;
}

void CEntryImpl::setDescription(const char *desc)
{
	description = desc;
}

void CEntryImpl::setDescription(const std::string &desc)
{
	description = desc;
}


void CEntryImpl::setCacheable(Cacheable cacheable)
{
	if ( UNKNOWN_CACHEABLE != getCacheable() && locked ) {
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable    = cacheable;
}

void CEntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}

Field IField::create(const char *name, uint64_t size)
{
	return CEntryImpl::create<CEntryImpl>(name, size);
}


