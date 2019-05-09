 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <cpsw_fs_addr.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>

#include <cpsw_yaml.h>

#undef HUB_DEBUG

using cpsw::static_pointer_cast;

static ByteOrder hbo()
{
union { uint16_t i; uint8_t c[2]; } tst = { i:1 };
	return tst.c[0] ? LE : BE;
}

static ByteOrder _hostByteOrder = hbo();

ByteOrder hostByteOrder() {  return _hostByteOrder; }

void _setHostByteOrder(ByteOrder o) { _hostByteOrder = o; }

CAddressImpl::CAddressImpl(AKey owner, unsigned nelms, ByteOrder byteOrder)
: owner_     ( owner                          ),
  child_     ( static_cast<CEntryImpl*>(NULL) ),
  nelms_     ( nelms                          ),
  isSync_    ( true                           ),
  byteOrder_ ( byteOrder                      )
{
	openCount_.store(0, cpsw::memory_order_release);

	if ( UNKNOWN == byteOrder )
		this->byteOrder_ = hostByteOrder();
}

CAddressImpl::CAddressImpl(AKey owner, YamlState &ypath)
: owner_     ( owner                          ),
  child_     ( static_cast<CEntryImpl*>(NULL) ),
  nelms_     ( IDev::DFLT_NELMS               ),
  isSync_    ( true                           ),
  byteOrder_ ( UNKNOWN                        )
{
	readNode(ypath, YAML_KEY_nelms, &nelms_);
	readNode(ypath, YAML_KEY_byteOrder, &byteOrder_ );

	openCount_.store(0, cpsw::memory_order_release);
	if ( UNKNOWN == byteOrder_ )
		byteOrder_ = hostByteOrder();
}

// The current implementation allows us to just 
// copy references. But that might change in the
// future if we decide to build a full copy of
// everything.
CAddressImpl::CAddressImpl(const CAddressImpl &orig, AKey new_owner)
:owner_    (new_owner      ),
 //child_(orig.child_), has no new child yet!
 nelms_    (orig.nelms_    ),
 isSync_   (orig.isSync_   ),
 byteOrder_(orig.byteOrder_)
{
	openCount_.store( orig.openCount_.load( cpsw::memory_order_acquire ), cpsw::memory_order_release);
}

AddressImpl CAddressImpl::clone(DevImpl new_owner)
{
CAddressImpl *p = clone( AKey( new_owner ) );
	if ( typeid(*p) != typeid(*this) ) {
		delete ( p );
		throw InternalError("Some subclass of CAddressImpl doesn't implement 'clone(AKey)'");
	}
	return AddressImpl(p);
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

double CAddressImpl::getPollSecs() const
{
	if ( ! child_ )
		throw InternalError("CAddressImpl: child pointer not set");
	return child_->getPollSecs();
}

uint64_t CAddressImpl::getSize() const
{
	if ( ! child_ )
		throw InternalError("CAddressImpl: child pointer not set");
	return child_->getSize();
}

int  CAddressImpl::incOpen()
{
	return openCount_.fetch_add(1, cpsw::memory_order_acq_rel );
}

int  CAddressImpl::decOpen()
{
	return openCount_.fetch_sub(1, cpsw::memory_order_acq_rel );
}


int  CAddressImpl::open(CompositePathIterator *node)
{
Address c;
	/* make sure parent is open */
	++(*node);
	if ( ! node->atEnd() ) {
		Address c = (*node)->c_p_;
		c->open( node );
	}

	/* return 'our' count */
	return incOpen();
}

int  CAddressImpl::close(CompositePathIterator *node)
{
int rval = decOpen();

	/* make sure parent is closed */
	++(*node);
	if ( ! node->atEnd() ) {
		Address c = (*node)->c_p_;
		c->close( node );
	}

	/* return 'our' count */
	return rval;
}

uint64_t  CAddressImpl::dispatchRead(CompositePathIterator *node, CReadArgs *args) const
{
	Address c;

#ifdef HUB_DEBUG
	printf("Reading %d bytes from %s (dispatching to parent)", args->nbytes_, getName());
	if ( getNelms() > 1 ) {
		printf("[%i", (*node)->idxf_);
		if ( (*node)->idxt_ > (*node)->idxf_ )
			printf("-%i", (*node)->idxt_);
		printf("]");
	}
	printf(" @%"PRIx64, args->off_);
	printf(" --> %p ", args->dst_);
	dump(); printf("\n");
#endif

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p_;
		return c->read(node, args);
	} else {
		throw ConfigurationError("Configuration Error: -- unable to dispatch read to parent");
		return 0;
	}
}

uint64_t  CAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
int f = (*node)->idxf_;
int t = (*node)->idxt_;

CReadArgs nargs = *args;
int       i;
uint64_t  rval = 0;
uint64_t  got;

#ifdef HUB_DEBUG
	printf("Reading %d bytes from %s", args->nbytes_, getName());
	if ( getNelms() > 1 ) {
		printf("[%i", (*node)->idxf_);
		if ( (*node)->idxt_ > (*node)->idxf_ )
			printf("-%i", (*node)->idxt_);
		printf("]");
	}
	printf(" @%"PRIx64, args->off_);
	printf(" --> %p ", args->dst_);
	dump(); printf("\n");
#endif

	nargs.off_ += f * args->nbytes_;

	for ( i = f; i <= t; i++ ) {
		got         = read( &nargs );
		rval       += got;
		nargs.off_ += got;
		if ( 0 != nargs.dst_ )
			nargs.dst_ += got;
	}

	if ( isSync_ && nargs.aio_ ) {
		nargs.aio_->callback( 0 );
	}
	return rval;
}

uint64_t  CAddressImpl::read(CReadArgs *args) const
{
	throw ConfigurationError("Configuration Error: -- unable to route I/O for read");
}

uint64_t CAddressImpl::dispatchWrite(CompositePathIterator *node, CWriteArgs *args) const
{
	Address c;

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p_;
		return c->write(node, args);
	} else {
		throw ConfigurationError("Configuration Error: -- unable to dispatch write to parent");
		return 0;
	}
}

uint64_t CAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
int        f = (*node)->idxf_;
int        t = (*node)->idxt_;
CWriteArgs nargs = *args;
int        i;
uint64_t   rval = 0;
uint64_t   put;

	nargs.off_ += f * args->nbytes_;

	for ( i = f; i <= t; i++ ) {
		checkWriteAlignmentReqs( &nargs );
		put         = write( &nargs );
		rval       += put;
		nargs.off_ += put;
		if ( 0 != nargs.src_ )
			nargs.src_ += put;
	}

	return rval;
}

uint64_t CAddressImpl::write(CWriteArgs *args) const
{
	throw ConfigurationError("Configuration Error: -- unable to route I/O for write");
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

	if ( e->getLocked() ) {
		child = e = CShObj::clone( e );
fprintf(stderr, "Cloned child %s of %s\n", child->getName(), getName());
	}

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
	setCacheable( WB_CACHEABLE );
}

CDevImpl::CDevImpl(Key &key, YamlState &ypath)
: CEntryImpl(key, ypath)
{
	setCacheable( WB_CACHEABLE ); // default for containers
}


CDevImpl::CDevImpl(const CDevImpl &orig, Key &k)
: CEntryImpl(orig, k)
{
	setCacheable( WB_CACHEABLE );

}

int
CDevImpl::getDefaultConfigPrio() const
{
	return DFLT_CONFIG_PRIO_DEV;
}

void
CDevImpl::postHook( ConstShObj orig )
{
	CEntryImpl::postHook( orig );

	/* can recreate the children only *after* the object is fully
	 * constructed (self pointer set)
	 */
	if ( orig ) {
		ConstDevImpl origDev( static_pointer_cast<ConstDevImpl::element_type>( orig ) );

		MyChildren::iterator it ( origDev->children_.begin() );
		MyChildren::iterator ite( origDev->children_.end()   );

		while ( it != ite ) {
			add(
					it->second->clone( getSelfAs<DevImpl>() ), 
					CShObj::clone( it->second->getEntryImpl() )
			   );
			++it;
		}
	}
}

void
CDevImpl::addAtAddress(Field child, unsigned nelms)
{
	IAddress::AKey k = getAKey();
	add( cpsw::make_shared<CAddressImpl>(k, nelms), child->getSelf() );
}

void
CDevImpl::addAtAddress(Field child, YamlState &ypath)
{
	try {
		doAddAtAddress<CFSAddressImpl>( child, ypath );
	} catch (NotFoundError &e) {
		doAddAtAddress<CAddressImpl>( child, ypath );
	}
}

void
CAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	writeNode(node, YAML_KEY_nelms,     nelms_    );
	writeNode(node, YAML_KEY_byteOrder, byteOrder_);
}

void
CAddressImpl::checkWriteAlignmentReqs(CWriteArgs *wargs) const
{
	if ( wargs->cacheable_ < IField::WB_CACHEABLE ) {
		unsigned msk = getAlignment() - 1;

		if ( wargs->msk1_ || wargs->mskn_  ) {
			throw IOError("CAddressImpl: Cannot merge bits to non-cacheable area");
		}
		if ( ((unsigned)wargs->off_ & msk) || ((unsigned)wargs->nbytes_ & msk) ) {
			throw IOError("CAddressImpl: Non-word aligned writes to non-cacheable area not permitted");
		}
	}
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
        YAML::Node child_address;
		it->second->dumpYamlPart( child_address );
        writeNode(child_node, YAML_KEY_at, child_address);
		if ( it->second->getEntryImpl()->getCacheable() == getCacheable() )
			child_node.remove(YAML_KEY_cacheable);
		writeNode(children, it->second->getName(), child_node );
	}
	writeNode( node, YAML_KEY_children, children );
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
Children rval( cpsw::make_shared<Children::element_type>( children_.size() ) );
int      i;

MyChildren::iterator it;

	// copy into a vector
	for ( it = children_.begin(), i=0; it != children_.end(); ++it, ++i ) {
		(*rval)[i] = it->second;
	}

	return rval;
}

uint64_t
CDevImpl::processYamlConfig(Path p, YAML::Node &n, bool doDump) const
{
const char *job  = doDump ? "'dump'" : "'load'";
uint64_t    rval = 0;

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
		rval += CEntryImpl::processYamlConfig( p, self, doDump );
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
			rval += a->getEntryImpl()->processYamlConfig( p, child, doDump );

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

			if ( 0 == std::string(YAML_KEY_value).compare( child.Tag() ) ) {
				// If this node has a 'value' tag then that means that the
				// node contains settings for *this* device (see above).
				// A subclass of CDevImpl may want to load/save state here...
				rval += CEntryImpl::processYamlConfig( p, child, doDump );
			} else {
				YAML::Mark mrk( child.Mark() );
				// no 'value' tag; this means that the child must be a map
				// with a single entry: a path element (key) which identifies
				// a descendant of ours and a value which is to be interpreted
				// by the descendant.
				if ( ! child.IsMap() ) {
					fprintf(stderr,"WARNING CDevImpl::processYamlConfig(%s) -- unexpected YAML node @line %d, col %d (Map expected) -- IGNORING\n", job, mrk.line, mrk.column);
					continue;
				}
				if ( child.size() != 1 ) {
					fprintf(stderr,"WARNING CDevImpl::processYamlConfig(%s) -- unexpected YAML node @line %d, col %d (Map with more than 1 element) -- IGNORING\n", job, mrk.line, mrk.column);
					continue;
				}

				// obtain the single entry of this map
				YAML::iterator item( child.begin() );

				// extract the key
				const std::string &key = item->first.as<std::string>();

				try {
					// try to find the entity referred to by the yaml node in our hierarchy
					Path descendant( findByName( key.c_str() ) );

					p->append( descendant );

					if ( doDump )
						rval += p->dumpConfigToYaml( item->second );
					else
						rval += p->loadConfigFromYaml( item->second );

					int i = descendant->size();

					// strip descendant from path again
					while ( i-- > 0 )
						p->up();

				} catch ( NotFoundError e ) {
					// descendant not found; not a big deal but spit out a warning
					fprintf(stderr,"WARNING CDevImpl::processYamlConfig(%s) -- unexpected YAML node @line %d, col %d (key %s not found) -- IGNORING\n", job, mrk.line, mrk.column, key.c_str());
					child.remove( key );
				}
			}

			// if the child has cleared 'its' node then we also remove the sequence entry
			if ( child && child.begin() == child.end() ) {
				child = YAML::Node( YAML::NodeType::Undefined );
			}
		}
	}
	return rval;
}
