#include <cpsw_hub.h>
#include <cpsw_path.h>

#include <inttypes.h>
#include <stdio.h>

static ByteOrder hbo()
{
union { uint16_t i; uint8_t c[2]; } tst = { i:1 };
	return tst.c[0] ? LE : BE;
}

static ByteOrder _hostByteOrder = hbo();

ByteOrder hostByteOrder() {  return _hostByteOrder; }

void _setHostByteOrder(ByteOrder o) { _hostByteOrder = o; }

Address::Address(Dev *owner, unsigned nelms, ByteOrder byteOrder)
:owner(owner), child(NULL), nelms(nelms), byteOrder(byteOrder)
{
	if ( UNKNOWN == byteOrder )
		byteOrder = hostByteOrder();
}

uint64_t  Address::read(CompositePathIterator *node, Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
	const Child *c;
#ifdef HUB_DEBUG
	printf("Reading %s", getName());
	if ( getNelms() > 1 ) {
		printf("[%i", (*node)->idxf);
		if ( (*node)->idxt > (*node)->idxf )
			printf("-%i", (*node)->idxt);
		printf("]");
	}
	printf(" @%"PRIx64, off);
	printf(" --> %p ", dst);
	dump(); printf("\n");
#endif

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p;
		return c->read(node, cacheable, dst, dbytes, off, sbytes);
	} else {
		throw ConfigurationError("Configuration Error: -- unable to route I/O");
		return 0;
	}
}

const IDev *Address::getOwner() const 
{
	return owner;
}

void Address::dump(FILE *f) const
{
	fprintf(f, "@%s:%s[%i]", owner->getName(), child->getName(), nelms);
}

class AddChildVisitor: public Visitor {
private:
	const Dev *parent;

public:
	AddChildVisitor(Dev *top, Entry *child) : parent(top)
	{
		child->accept( this, false );
	}

	virtual void visit(const Entry *child) {
//		printf("considering propagating atts to %s\n", child->getName());
		if ( ! parent ) {
			throw InternalError("InternalError: AddChildVisitor has no parent");
		}
		if ( UNKNOWN_CACHEABLE != parent->getCacheable() && UNKNOWN_CACHEABLE == child->getCacheable() ) {
//			printf("setting cacheable\n");
			child->setCacheable( parent->getCacheable() );
		}
	}

	virtual void visit(const Dev *child) {
		if ( UNKNOWN_CACHEABLE == child->getCacheable() )
			visit( (const Entry*) child );
		parent = child;
//		printf("setting parent to %s\n", child->getName());
	}
};

void Dev::add(Address *a, Entry *child)
{
	AddChildVisitor propagateAttributes( this, child );

	child->Entry::setLocked(); //printf("locking %s\n", child->getName());
	a->attach( child );
	std::pair<Children::iterator,bool> ret = children.insert( std::pair<const char *, Address*>(a->getName(), a) );
	if ( ! ret.second ) {
		delete a;
		throw DuplicateNameError(child->getName());
	}
}

Dev::~Dev()
{
Children::iterator it;

	while ( (it = children.begin()) != children.end() ) {
		children.erase( it );
		delete( it->second );
	}
}

const Child *Dev::getChild(const char *name) const
{
	return children[name];
}

Dev::Dev(const char *name, uint64_t size)
: Entry(name, size)
{
	// by default - mark containers as write-through cacheable; user may still override
	setCacheable( WT_CACHEABLE );
}

Path Dev::findByName(const char *s) const
{
	Path p = IPath::create( this );
	return p->findByName( s );
}

void Dev::accept(Visitor *v, bool depthFirst) const
{
Children::iterator it;

	if ( ! depthFirst ) {
		v->visit( this );
	}

	for ( it = children.begin(); it != children.end(); ++it ) {
		const Entry *e = it->second->getEntry();
		e->accept( v, depthFirst );
	}

	if ( depthFirst ) {
		v->visit( this );
	}
}
