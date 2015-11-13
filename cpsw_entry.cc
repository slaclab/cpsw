#include <api_builder.h>
#include <cpsw_hub.h>
#include <ctype.h>

int cpsw_obj_count = 0;

EntryImpl::EntryImpl(FKey k, uint64_t size)
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

EntryImpl::~EntryImpl()
{
	cpsw_obj_count--;
}

void EntryImpl::setCacheable(Cacheable cacheable)
{
	if ( UNKNOWN_CACHEABLE != getCacheable() && locked ) {
	printf("%s\n", getName());
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable    = cacheable;
}

void EntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}
