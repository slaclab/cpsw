#include <cpsw_api_builder.h>
#include <cpsw_hub.h>
#include <ctype.h>

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

CEntryImpl::CEntryImpl(const CEntryImpl &ei)
: name(ei.name),
  description(ei.description),
  size(ei.size),
  cacheable(ei.cacheable),
  locked(ei.locked)
{
	self.reset();
}

CEntryImpl & CEntryImpl::operator=(const CEntryImpl &in)
{
	if ( this != &in ) {
		name        = in.name;
		description = in.description;
		size        = in.size;
		cacheable   = in.cacheable;
		locked      = in.locked;
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
	printf("%s\n", getName());
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable    = cacheable;
}

void CEntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}
