#include <cpsw_hub.h>
#include <cpsw_path.h>

#include <inttypes.h>
#include <stdio.h>

int  Address::read(CompositePathIterator *node, uint8_t *dst, uint64_t off, int headBits, uint64_t sizeBits) const
{
	const Child *c;
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

	// chain through parent
	++(*node);
	if ( ! node->atEnd() ) {
		c = (*node)->c_p;
		return c->read(node, dst, off, headBits, sizeBits);
	} else {
		printf("FIXME -- 'read' should never get here\n");
		return 0;
	}
}

void Address::dump(FILE *f) const
{
	fprintf(f, "@%s:%s[%i]", owner->getName(), child->getName(), nelms);
}

void Dev::add(Address *a, Entry *child)
{
	a->attach( child );
	std::pair<Children::iterator,bool> ret = children.insert( std::pair<const char *, Address*>(a->getName(), a) );
	if ( ! ret.second ) {
		delete a;
		throw DuplicateNameError(child->getName());
	}
}

const Child *Dev::getChild(const char *name) const
{
	return children[name];
}

Dev::Dev(const char *name, uint64_t sizeBits)
: Hub(name, sizeBits)
{
}

Path Dev::findByName(const char *s) const
{
	Path p = PathInterface::create( this );
	return p->findByName( s );
}
