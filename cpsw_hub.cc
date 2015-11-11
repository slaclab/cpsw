#include <cpsw_hub.h>
#include <cpsw_path.h>

#include <inttypes.h>
#include <stdio.h>

using boost::static_pointer_cast;

static ByteOrder hbo()
{
union { uint16_t i; uint8_t c[2]; } tst = { i:1 };
	return tst.c[0] ? LE : BE;
}

static ByteOrder _hostByteOrder = hbo();

ByteOrder hostByteOrder() {  return _hostByteOrder; }

void _setHostByteOrder(ByteOrder o) { _hostByteOrder = o; }

AddressImpl::AddressImpl(AKey owner, unsigned nelms, ByteOrder byteOrder)
:owner(owner), child( static_cast<EntryImpl*>(NULL) ), nelms(nelms), byteOrder(byteOrder)
{
	if ( UNKNOWN == byteOrder )
		this->byteOrder = hostByteOrder();
}

uint64_t  AddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
	Child c;
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

Container AddressImpl::getOwner() const
{
	return owner.get();
}

void AddressImpl::dump(FILE *f) const
{
	fprintf(f, "@%s:%s[%i]", getOwner()->getName(), child->getName(), nelms);
}

class AddChildVisitor: public IVisitor {
private:
	Dev parent;

public:
	AddChildVisitor(Dev top, Field child) : parent(top)
	{
		child->accept( this, IVisitable::RECURSE_DEPTH_AFTER, IVisitable::DEPTH_INDEFINITE );
	}

	virtual void visit(Field child) {
//		printf("considering propagating atts to %s\n", child->getName());
		if ( ! parent ) {
			throw InternalError("InternalError: AddChildVisitor has no parent");
		}
		if ( IField::UNKNOWN_CACHEABLE != parent->getCacheable() && IField::UNKNOWN_CACHEABLE == child->getCacheable() ) {
//			printf("setting cacheable\n");
			child->setCacheable( parent->getCacheable() );
		}
	}

	virtual void visit(Dev child) {
		if ( IField::UNKNOWN_CACHEABLE == child->getCacheable() )
			visit( static_pointer_cast<IField, IDev>( child ) );
		parent = child;
//		printf("setting parent to %s\n", child->getName());
	}

};

void DevImpl::add(shared_ptr<AddressImpl> a, Field child)
{
Entry e = child->getSelf();

	AddChildVisitor propagateAttributes( getSelfAs<Container>(), child );

	e->setLocked(); //printf("locking %s\n", child->getName());
	a->attach( e );
	std::pair<Children::iterator,bool> ret = children.insert( std::pair<const char *, shared_ptr<AddressImpl> >(child->getName(), a) );
	if ( ! ret.second ) {
		/* Address object should be automatically deleted by smart pointer */
		throw DuplicateNameError(child->getName());
	}
}

DevImpl::~DevImpl()
{
}

Child DevImpl::getChild(const char *name) const
{
	return children[name];
}

DevImpl::DevImpl(FKey k, uint64_t size)
: EntryImpl(k, size)
{
	// by default - mark containers as write-through cacheable; user may still override
	EntryImpl::setCacheable( WT_CACHEABLE );
}

Dev IDev::create(const char *name, uint64_t size)
{
	return EntryImpl::create<DevImpl>(name, size);
}

Field IField::create(const char *name, uint64_t size)
{
	return EntryImpl::create<EntryImpl>(name, size);
}

Path DevImpl::findByName(const char *s)
{
	Hub  h( getSelfAs<Container>() );
	Path p = IPath::create( h );
	return p->findByName( s );
}

void DevImpl::accept(IVisitor    *v, RecursionOrder order, int recursionDepth)
{
Children::iterator it;
Dev       meAsDev( getSelfAs<Container>() );

	if ( RECURSE_DEPTH_FIRST != order ) {
		v->visit( meAsDev );
	}

	if ( DEPTH_NONE != recursionDepth ) {
		if ( IField::DEPTH_INDEFINITE != recursionDepth ) {
			recursionDepth--;
		}
		for ( it = children.begin(); it != children.end(); ++it ) {
			Entry e = it->second->getEntry();
			e->accept( v, order, recursionDepth );
		}
	}

	if ( RECURSE_DEPTH_FIRST == order ) {
		v->visit( meAsDev );
	}
}

IntEntryImpl::IntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit, int wordSwap)
: EntryImpl(k, (sizeBits + lsBit + 7)/8), is_signed(is_signed), ls_bit(lsBit), size_bits(sizeBits), wordSwap(wordSwap)
{
}

IntField IIntField::create(const char *name, uint64_t sizeBits, bool is_signed, int lsBit, int wordSwap)
{
	return EntryImpl::create<IntEntryImpl>(name, sizeBits, is_signed, lsBit, wordSwap);
}

