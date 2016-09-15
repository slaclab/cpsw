 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_stream_adapt.h>
#include <cpsw_hub.h>
#include <ctype.h>

#include <cpsw_obj_cnt.h>

#include <stdint.h>

#include <cpsw_yaml.h>

using boost::dynamic_pointer_cast;
using boost::make_shared;

const int CEntryImpl::CONFIG_PRIO_OFF;
const int CEntryImpl::DFLT_CONFIG_PRIO;

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
  configPrio_( DFLT_CONFIG_PRIO ),
  configPrioSet_( false ),
  locked_( false )
{
	checkArgs();
	++ocnt();
}

CEntryImpl::CEntryImpl(const CEntryImpl &ei, Key &k)
: CShObj(ei,k),
  name_(ei.name_),
  description_(ei.description_),
  size_(ei.size_),
  cacheable_( DFLT_CACHEABLE ),      // reset copy to default
  configPrio_( DFLT_CONFIG_PRIO ),   // reset copy to default
  configPrioSet_( false ),           // reset copy to default
  locked_( false )
{
	++ocnt();
}

CEntryImpl::CEntryImpl(Key &key, YamlState &ypath)
: CShObj(key),
  name_( ypath.getName() ),
  size_( DFLT_SIZE ),
  cacheable_( DFLT_CACHEABLE ),
  configPrio_( DFLT_CONFIG_PRIO ),
  configPrioSet_( false ),
  locked_( false )
{

	{
	std::string str;
	if ( readNode( ypath, YAML_KEY_description, &str ) )
		setDescription( str );
	}

	readNode( ypath, YAML_KEY_size, &size_ );

	{
	IField::Cacheable cacheable;
	if ( readNode( ypath, YAML_KEY_cacheable, &cacheable ) )
		setCacheable( cacheable );
	}

	int configPrio;
	if ( readNode( ypath, YAML_KEY_configPrio, &configPrio ) )
		setConfigPrio( configPrio );

	checkArgs();

	++ocnt();
}

void
CEntryImpl::postHook( ConstShObj orig )
{
	// executed once the object is fully constructed;
	// we may thus use polymorphic function here...
	if ( ! configPrioSet_ )
		setConfigPrio( getDefaultConfigPrio() );
}

// Quite dumb ATM; no attempt to consolidate
// identical nodes.
void CEntryImpl::dumpYamlPart(YAML::Node &node) const
{
const char *d = getDescription();

	// chain through superclass
	CYamlSupportBase::dumpYamlPart( node );

	if ( d && strlen(d) > 0 ) {
		writeNode(node, YAML_KEY_description, d);
	}

	if ( getSize() != DFLT_SIZE )
		writeNode(node, YAML_KEY_size, getSize());

	if ( getCacheable() != DFLT_CACHEABLE )
		writeNode(node, YAML_KEY_cacheable, getCacheable());

	if ( getConfigPrio() != getDefaultConfigPrio() )
		writeNode(node, YAML_KEY_configPrio, getConfigPrio());

}

int CEntryImpl::getDefaultConfigPrio() const
{
	return DFLT_CONFIG_PRIO;
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

void CEntryImpl::setConfigPrio(int configPrio)
{
	if ( configPrio_ != configPrio && locked_ ) {
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	}
	this->configPrio_    = configPrio;
	this->configPrioSet_ = true;
}

void CEntryImpl::processYamlConfig(Path p, YAML::Node &n, bool doDump) const
{
	if ( doDump ) {
		YAML::Node new_n = dumpMyConfigToYaml( p );
		if ( new_n ) {
			// attach a tag.
			new_n.SetTag( YAML_KEY_value );
		}
		n = new_n;
	} else {
		loadMyConfigFromYaml(p, n);
	}
}

YAML::Node CEntryImpl::dumpMyConfigToYaml(Path p) const
{
	// normally, entries which have nothing to save/restore
	// should not even be executing this. It may however
	// happen that a config file template lists an entry
	// which has nothing to contribute.
	// We just return an empty node which tells our
	// caller to ignore us.
	return YAML::Node( YAML::NodeType::Undefined );
}

void
CEntryImpl::loadMyConfigFromYaml(Path p, YAML::Node &n) const
{
	throw ConfigurationError("This class doesn't implement loadMyConfigFromYaml");
}

void CEntryImpl::accept(IVisitor *v, RecursionOrder order, int depth)
{
	v->visit( getSelf() );
}

Field IField::create(const char *name, uint64_t size)
{
	return CShObj::create<EntryImpl>(name, size);
}

CEntryImpl::CUniqueListHead::~CUniqueListHead()
{
	if ( n_ )
		throw InternalError("UniqueListHead not empty on destruction!");
}

class CEntryImpl::CUniqueHandle : public CEntryImpl::CUniqueListHead {
private:
	CUniqueListHead  *p_;
	CMtx             *mtx_;
	ConstPath         path_;

public:
	CUniqueHandle(CMtx *m, ConstPath path)
	: CUniqueListHead(),
	  p_(0),
	  mtx_(m),
	  path_(path)
	{
	}

	// MUST be called from mutex - guarded code!
	void add_unguarded(CUniqueListHead *h)
	{
		if ( (n_ = h->getNext()) )
			n_->p_ = this;
		p_    = h;
		h->setNext( this );
	}

	void del_unguarded()
	{
		if ( p_ )
			p_->setNext( n_ );
		if ( n_ )
			n_->p_ = p_;
		n_ = 0;
		p_ = 0;
	}

	ConstPath getConstPath()
	{
		return path_;
	}

	~CUniqueHandle()
	{
	CMtx::lg guard( mtx_ );
		del_unguarded();
	}
};

CEntryImpl::UniqueHandle
CEntryImpl::getUniqueHandle(IEntryAdapterKey &k, ConstPath path) const
{
CMtx *mtx = uniqueListMtx_.getMtx();

CMtx::lg guard( mtx );

CUniqueHandle *nod = uniqueListHead_.getNext();

	while ( nod ) {
		if ( nod->getConstPath()->isIntersecting( path ) ) {
			// must not instantiate this interface multiple times
			throw MultipleInstantiationError( path->toString() );
		}

		nod = nod->getNext();
	}

	UniqueHandle h = make_shared<CUniqueHandle>( mtx, path );

	// still protected by the guard (see above)
	h->add_unguarded( &uniqueListHead_ );

	return h;
}

EntryAdapt
CEntryImpl::createAdapter(IEntryAdapterKey &key, Path p, const std::type_info &interfaceType) const
{
	if ( isInterface<Stream>(interfaceType) ) {
		_createAdapter<StreamAdapt>(this, p);
	}
	return EntryAdapt();
}
