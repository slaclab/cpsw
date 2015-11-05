#include <api_user.h>
#include <cpsw_hub.h>
#include <ctype.h>

Entry::Entry(const char *cname, uint64_t size)
: name( cname ),
  size( size),
  cacheableSet( false ),
  locked( false )
{
const char *cptr;
	for ( cptr = cname; *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(cname);
	}
}

void Entry::setCacheable(bool cacheable) const
{
	if ( cacheableSet && locked ) {
	printf("%s\n", getName());
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable    = cacheable;
	this->cacheableSet = true;
}

void Entry::accept(Visitor *v, bool depthFirst) const
{
	v->visit( this );
}
