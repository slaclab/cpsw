#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <ctype.h>

#include <stdint.h>

int cpsw_obj_count = 0;

CEntryImpl::CEntryImpl(FKey k, uint64_t size)
: name_( k.getName() ),
  size_( size),
  cacheable_( UNKNOWN_CACHEABLE ),
  locked_( false )
{
const char *cptr;
	for ( cptr = name_.c_str(); *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(name_);
	}
	cpsw_obj_count++;
}

CEntryImpl::CEntryImpl(CEntryImpl &ei)
: name_(ei.name_),
  description_(ei.description_),
  size_(ei.size_),
  cacheable_(ei.cacheable_),
  locked_( false )
{
	self_.reset();
}

CEntryImpl & CEntryImpl::operator=(CEntryImpl &in)
{
	if ( locked_ ) {
		throw ConfigurationError("Must not assign to attached EntryImpl");
	}
	if ( this != &in ) {
		name_       = in.name_;
		description_= in.description_;
		size_       = in.size_;
		cacheable_  = in.cacheable_;
		locked_     = false;
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
	description_ = desc;
}

void CEntryImpl::setDescription(const std::string &desc)
{
	description_ = desc;
}


void CEntryImpl::setCacheable(Cacheable cacheable)
{
	if ( UNKNOWN_CACHEABLE != getCacheable() && locked_ ) {
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->cacheable_   = cacheable;
}

void CEntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}

Field IField::create(const char *name, uint64_t size)
{
	return CEntryImpl::create<CEntryImpl>(name, size);
}


