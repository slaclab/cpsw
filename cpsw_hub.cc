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
int       prio;

	AddChildVisitor propagateAttributes( getSelfAs<DevImpl>(), child );

	e->setLocked(); //printf("locking %s\n", child->getName());
	a->attach( e );
	std::pair<MyChildren::iterator,bool> ret = children_.insert( std::pair<const char *, AddressImpl>(child->getName(), a) );
	if ( ! ret.second ) {
		/* Address object should be automatically deleted by smart pointer */
		throw DuplicateNameError(child->getName());
	}

	// add to the prioritiy list; priority 0 means no dumping
	if ( (prio = e->getConfigPrio()) ) {
		if ( configPrioList_.empty() || prio <= configPrioList_.front()->getEntryImpl()->getConfigPrio() ) {
			configPrioList_.push_front( a );
		} else if ( prio >= configPrioList_.back()->getEntryImpl()->getConfigPrio() ) {
			configPrioList_.push_back( a );
		} else {
			// must search the list;
			PrioList::iterator it( configPrioList_.begin() );
			
			while ( (*it)->getEntryImpl()->getConfigPrio() < prio ) {
				++it;
			}
			configPrioList_.insert( it, a );
		}
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

void CDevImpl::processYamlConfig(Path p, YAML::Node &n, bool doDump) const
{
	if ( !n || n.IsNull() ) {
		// new node, i.e., first time config

		if ( ! doDump )
			throw ConfigurationError("No YAML node to process (loadConfig)");


		// construct an empty node for our children
		n = YAML::Node( YAML::NodeType::Undefined );

		// first, we dump any settings of our own.
		// CDevImpl does have no such settings but
		// a subclass may and override 'dumpYamlConfig()/loadYamlConfig()'.
		// Such settings would end up in the 'self' node.
		YAML::Node self;
		CEntryImpl::processYamlConfig( p, self, doDump );
		if ( self && ! self.IsNull() ) {
			// only save 'self' if there is anything...
			n.push_back(self);
		}

		// now proceed to saving all our children
		PrioList::const_iterator it;
		for ( it=configPrioList_.begin(); it != configPrioList_.end(); ++it ) {
			Address a( *it );

			// A node for the child
			YAML::Node child;

			// append child to the path
			IPathImpl::toPathImpl( p )->append( a, 0, a->getNelms() - 1 );

			// dump child into the 'child' node
			a->getEntryImpl()->processYamlConfig( p, child, doDump );

			if ( child && ! child.IsNull() ) {
				// they actually put something there;

				// now create a single-element map; the 'key' is the pathname
				// of the child and the value is the child node containing
				// all of the child's settings
				YAML::Node path_node;
				path_node[a->getName()] = child;

				// append to our sequence
				n.push_back(path_node);
			}

			// pop child from path
			p->up();
		}
	} else if ( ! n.IsSequence() ) {
		throw ConfigurationError("Unexpected YAML format -- sequence expected");
	} else {
		// We did get a template hierarchy. OK; we'll save stuff in the order
		// specified by the template sequence.
		YAML::Node::iterator it;
		for ( it = n.begin(); it != n.end(); ++it ) {

			YAML::Node child(*it);

			if ( 0 == std::string("value").compare( child.Tag() ) ) {
				// If this node has a 'value' tag then that means that the
				// node contains settings for *this* device (see above).
				// A subclass of CDevImpl may want to load/save state here...
				CEntryImpl::processYamlConfig( p, child, doDump );
			} else {
				// no 'value' tag; this means that the child must be a map
				// with a single entry: a path element (key) which identifies
				// a descendant of ours and a value which is to be interpreted
				// by the descendant.
				if ( ! child.IsMap() ) {
					fprintf(stderr,"WARNING CDevImpl::dumpConfigToYaml -- unexpected YAML node '%s' (Map expected) -- IGNORING\n", child.as<std::string>().c_str());
					continue;
				}
				if ( child.size() != 1 ) {
					fprintf(stderr,"WARNING CDevImpl::dumpConfigToYaml -- unexpected YAML node '%s' (Map with more than 1 element) -- IGNORING\n", child.as<std::string>().c_str());
					continue;
				}

				// obtain the single entry of this map
				YAML::iterator item( child.begin() );

				// extract the key
				const char *key = item->first.as<std::string>().c_str();

				try {
					// try to find the entity referred to by the yaml node in our hierarchy
					Path descendant( findByName( key ) );

					p->append( descendant );

					if ( doDump )
						p->dumpConfigToYaml( item->second );
					else
						p->loadConfigFromYaml( item->second );

					int i = descendant->size();

					// strip descendant from path again
					while ( i-- > 0 )
						p->up();

				} catch ( NotFoundError e ) {
					// descendant not found; not a big deal but spit out a warning
					fprintf(stderr,"WARNING CDevImpl::dumpConfigToYaml -- unexpected YAML node '%s' (key %s not found) -- IGNORING\n", child.as<std::string>().c_str(), key);
				}
			}

			// if the child has cleared 'its' node then we also remove the sequence entry
			if ( child && child.begin() == child.end() )
				child = YAML::Node( YAML::NodeType::Undefined );
		}
	}
}
