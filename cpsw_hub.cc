#include <cpsw_api_builder.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>

#include <cpsw_yaml.h>

using boost::static_pointer_cast;
using boost::make_shared;

static ByteOrder hbo()
{
union { uint16_t i; uint8_t c[2]; } tst = { i:1 };
	return tst.c[0] ? LE : BE;
}

static ByteOrder _hostByteOrder = hbo();

ByteOrder hostByteOrder() {  return _hostByteOrder; }

void _setHostByteOrder(ByteOrder o) { _hostByteOrder = o; }

CAddressImpl::CAddressImpl(AKey owner, unsigned nelms, ByteOrder byteOrder)
:owner_(owner), child_( static_cast<CEntryImpl*>(NULL) ), nelms_(nelms), byteOrder_(byteOrder)
{
	if ( UNKNOWN == byteOrder )
		this->byteOrder_ = hostByteOrder();
}

// The current implementation allows us to just 
// copy references. But that might change in the
// future if we decide to build a full copy of
// everything.
CAddressImpl::CAddressImpl(CAddressImpl &orig)
:owner_(orig.owner_),
 child_(orig.child_),
 nelms_(orig.nelms_),
 byteOrder_(orig.byteOrder_)
{
}

CAddressImpl & CAddressImpl::operator=(CAddressImpl &orig)
{
	return (*this = orig);
}

Address CAddressImpl::clone(DevImpl new_owner)
{
IAddress *p = clone( AKey( new_owner ) );
	if ( typeid(*p) != typeid(*this) ) {
		delete ( p );
		throw InternalError("Some subclass of IAddress doesn't implement 'clone(AKey)'");
	}
	return Address(p);
}

Hub CAddressImpl::isHub() const
{
	if ( this->child_ == 0 )
		return NULLHUB;
	else	
		return this->child_->isHub();
}

void CAddressImpl::attach(EntryImpl child)
{
	if ( this->child_ != NULL ) {
		throw AddressAlreadyAttachedError( child->getName() );
	}
	this->child_ = child;
}

const char * CAddressImpl::getName() const
{
	if ( ! child_ )
		throw InternalError("CAddressImpl: child pointer not set");
	return child_->getName();
}

const char * CAddressImpl::getDescription() const
{
	if ( ! child_ )
		throw InternalError("CAddressImpl: child pointer not set");
	return child_->getDescription();
}

uint64_t CAddressImpl::getSize() const
{
	if ( ! child_ )
		throw InternalError("CAddressImpl: child pointer not set");
	return child_->getSize();
}

uint64_t  CAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
	Address c;
#ifdef HUB_DEBUG
	printf("Reading %s", getName());
	if ( getNelms() > 1 ) {
		printf("[%i", (*node)->idxf_);
		if ( (*node)->idxt_ > (*node)->idxf_ )
			printf("-%i", (*node)->idxt_);
		printf("]");
	}
	printf(" @%"PRIx64, off);
	printf(" --> %p ", dst);
	dump(); printf("\n");
#endif

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p_;
		return c->read(node, args);
	} else {
		throw ConfigurationError("Configuration Error: -- unable to route I/O for read");
		return 0;
	}
}

uint64_t CAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
	Address c;

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p_;
		return c->write(node, args);
	} else {
		throw ConfigurationError("Configuration Error: -- unable to route I/O for write");
		return 0;
	}
}

Hub CAddressImpl::getOwner() const
{
	return owner_.get();
}

DevImpl CAddressImpl::getOwnerAsDevImpl() const
{
	return owner_.get();
}

void CAddressImpl::dump(FILE *f) const
{
	fprintf(f, "@%s:%s[%i]", getOwner()->getName(), child_->getName(), nelms_);
}

class AddChildVisitor: public IVisitor {
private:
	Dev parent_;

public:
	AddChildVisitor(Dev top, Field child) : parent_(top)
	{
		child->accept( this, IVisitable::RECURSE_DEPTH_AFTER, IVisitable::DEPTH_INDEFINITE );
	}

	virtual void visit(Field child) {
//		printf("considering propagating atts to %s\n", child->getName());
		if ( ! parent_ ) {
			throw InternalError("InternalError: AddChildVisitor has no parent");
		}
		if ( IField::UNKNOWN_CACHEABLE != parent_->getCacheable() && IField::UNKNOWN_CACHEABLE == child->getCacheable() ) {
//			printf("setting cacheable\n");
			child->setCacheable( parent_->getCacheable() );
		}
	}

	virtual void visit(Dev child) {
		if ( IField::UNKNOWN_CACHEABLE == child->getCacheable() )
			visit( static_pointer_cast<IField, IDev>( child ) );
		parent_ = child;
//		printf("setting parent to %s\n", child->getName());
	}

};

void CDevImpl::add(AddressImpl a, Field child)
{
EntryImpl e = child->getSelf();

	AddChildVisitor propagateAttributes( getSelfAs<DevImpl>(), child );

	e->setLocked(); //printf("locking %s\n", child->getName());
	a->attach( e );
	std::pair<MyChildren::iterator,bool> ret = children_.insert( std::pair<const char *, AddressImpl>(child->getName(), a) );
	if ( ! ret.second ) {
		/* Address object should be automatically deleted by smart pointer */
		throw DuplicateNameError(child->getName());
	}
}

Hub CDevImpl::isHub() const
{
	return getSelfAsConst<DevImpl>();
}


CDevImpl::~CDevImpl()
{
}

Address CDevImpl::getAddress(const char *name) const
{
MyChildren::const_iterator it( children_.find( name ) );
	// operator[] creates an entry if the key is not found
	if ( it == children_.end() )
		return Address();
	return it->second;
}

CDevImpl::CDevImpl(Key &k, const char *name, uint64_t size)
: CEntryImpl(k, name, size)
{
	// by default - mark containers as write-through cacheable; user may still override
	setCacheable( WT_CACHEABLE );
}

CDevImpl::CDevImpl(Key &key, YamlState &ypath)
: CEntryImpl(key, ypath)
{
	setCacheable(WT_CACHEABLE); // default for containers
}

void
CDevImpl::addAtAddress(Field child, YamlState &ypath)
{
unsigned nelms = DFLT_NELMS;

	readNode(ypath, "nelms", &nelms);

	addAtAddress(child, nelms);
}

void
CAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	writeNode(node, "nelms",     nelms_    );
	writeNode(node, "ByteOrder", byteOrder_);
}

void
CDevImpl::dumpYamlPart(YAML::Node &node) const
{
MyChildren::iterator it;
	CEntryImpl::dumpYamlPart(node);
	YAML::Node children;
	for ( it = children_.begin(); it != children_.end(); ++it ) {
		YAML::Node child_node;
		it->second->getEntryImpl()->dumpYaml( child_node );
		it->second->dumpYamlPart( child_node );
		if ( it->second->getEntryImpl()->getCacheable() == getCacheable() )
			child_node.remove("cacheable");
		writeNode(children, it->second->getName(), child_node );
	}
	writeNode( node, "children", children );
}

Dev IDev::create(const char *name, uint64_t size)
{
	return CShObj::create<DevImpl>(name, size);
}

Path CDevImpl::findByName(const char *s) const
{
	Hub  h( getSelfAsConst<DevImpl>() );
	Path p = IPath::create( h );
	return p->findByName( s );
}

void CDevImpl::accept(IVisitor    *v, RecursionOrder order, int recursionDepth)
{
MyChildren::iterator it;
Dev       meAsDev( getSelfAs<DevImpl>() );

	if ( RECURSE_DEPTH_FIRST != order ) {
		v->visit( meAsDev );
	}

	if ( DEPTH_NONE != recursionDepth ) {
		if ( IField::DEPTH_INDEFINITE != recursionDepth ) {
			recursionDepth--;
		}
		for ( it = children_.begin(); it != children_.end(); ++it ) {
			EntryImpl e = it->second->getEntryImpl();
			e->accept( v, order, recursionDepth );
		}
	}

	if ( RECURSE_DEPTH_FIRST == order ) {
		v->visit( meAsDev );
	}
}

Children CDevImpl::getChildren() const
{
Children rval( make_shared<Children::element_type>( children_.size() ) );
int      i;

MyChildren::iterator it;

	// copy into a vector
	for ( it = children_.begin(), i=0; it != children_.end(); ++it, ++i ) {
		(*rval)[i] = it->second;
	}

	return rval;
}
