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
  locked( false )
{
	self.reset();
}

CEntryImpl & CEntryImpl::operator=(const CEntryImpl &in)
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
	printf("%s\n", getName());
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

static uint64_t b2B(uint64_t bits)
{
	return (bits + 7)/8;
}

CIntEntryImpl::CIntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
: CEntryImpl(
		k,
		wordSwap > 0 && wordSwap != b2B(sizeBits) ? b2B(sizeBits) + (lsBit ? 1 : 0) : b2B(sizeBits + lsBit)
	),
	is_signed(is_signed),
	ls_bit(lsBit), size_bits(sizeBits),
	mode(mode),
	wordSwap(wordSwap)
{
unsigned byteSize = b2B(sizeBits);

	if ( wordSwap == byteSize )
		wordSwap = this->wordSwap = 0;

	if ( wordSwap > 0 ) {
		if ( ( byteSize % wordSwap ) != 0 ) {
			throw InvalidArgError("wordSwap does not divide size");
		}
	}
}

IntField IIntField::create(const char *name, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
{
	return CEntryImpl::create<CIntEntryImpl>(name, sizeBits, is_signed, lsBit, mode, wordSwap);
}

