#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <cpsw_hub.h>
#include <ctype.h>

#include <cpsw_obj_cnt.h>

#include <stdint.h>

#include <cpsw_yaml.h>

using boost::dynamic_pointer_cast;

static DECLARE_OBJ_COUNTER( ocnt, "EntryImpl", 1 ) // root device

void CEntryImpl::checkArgs()
{
const char *cptr;
	for ( cptr = name_.c_str(); *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(name_);
	}
}

CEntryImpl::CEntryImpl(Key &k, const char *name, uint64_t size)
: CShObj(k),
  name_( name ),
  size_( size),
  cacheable_( DFLT_CACHEABLE ),
  locked_( false )
{
	checkArgs();
	++ocnt();
}

CEntryImpl::CEntryImpl(CEntryImpl &ei, Key &k)
: CShObj(ei,k),
  name_(ei.name_),
  description_(ei.description_),
  size_(ei.size_),
  cacheable_(ei.cacheable_),
  locked_( false )
{
	++ocnt();
}

CEntryImpl::CEntryImpl(Key &key, const YAML::Node &node)
: CShObj(key),
  size_( DFLT_SIZE ),
  cacheable_( DFLT_CACHEABLE ),
  locked_( false )
{

	mustReadNode( node, "name", &name_ );

	{
	const YAML::Node & n( getNode(node,"description") );
	if ( n ) {
		setDescription( n.as<std::string>() );
	}
	}

	readNode( node, "size", &size_ );

	{
	const YAML::Node & n( getNode(node, "cacheable") );
	if ( n ) {
		setCacheable( n.as<IField::Cacheable>() );
	}
	}

	checkArgs();

	++ocnt();
}

// Quite dumb ATM; no attempt to consolidate
// identical nodes.
void CEntryImpl::dumpYamlPart(YAML::Node &node) const
{
const char *d = getDescription();

	// chain through superclass
	CYamlSupportBase::dumpYamlPart( node );

	node["name"] = getName();

	if ( d && strlen(d) > 0 ) {
		node["description"] = d;
	}

	if ( getSize() != DFLT_SIZE )
		node["size"] = getSize();

	if ( getCacheable() != DFLT_CACHEABLE )
		node["cacheable"] = getCacheable();
}

CEntryImpl::~CEntryImpl()
{
	--ocnt();
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
	return CShObj::create<EntryImpl>(name, size);
}
