#include <cpsw_api_builder.h>
#include <stdio.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <cpsw_obj_cnt.h>
#include <iostream>

using std::cout;

class DVisitor : public IVisitor {
	public:
	virtual void visit(Field f) { printf("Visiting default E (%s)\n", f->getName()); }
	virtual void visit(Dev   d) { printf("Visiting default D (%s)\n", d->getName()); }
};

class DumpNameVisitor : public IVisitor {
public:
	virtual void visit(Field e) { printf("E: %s\n", e->getName()); }
}; 

struct TestFailed {};

static void recurse(Hub h, unsigned l)
{
unsigned i;
Children cc = h->getChildren();
Hub      hh;
	printf("%*s%s\n", l, "", h->getName());
	l++;
	for ( i=0; i<cc->size(); i++ ) {
		if ( (hh = (*cc)[i]->isHub()) ) { 
			recurse( hh, l );
		} else {
			printf("%*s%s\n", l, "", (*cc)[i]->getName());
		}
	}
}

int
main(int argc, char **argv)
{

try {
Path    p  = IPath::create();
Dev     r  = IDev::create("root");
	printf("root  use-count: %li\n", r.use_count());
Dev    c1 = IDev::create("outer");
Dev    c2 = IDev::create("inner");
Field  c3 = IField::create("leaf", 7);
Field  c4 = IField::create("leaf1", 8);

	c2->addAtAddress( c3 );
	c2->addAtAddress( c4, 4 );

	c2->setCacheable( IField::WT_CACHEABLE );
	c1->addAtAddress( c2, 4 );
	r->addAtAddress( c1,2 );

	c1.reset();
	c2.reset();
	c3.reset();
	c4.reset();

	printf("root  use-count: %li\n", r.use_count());
	{
	DumpNameVisitor v;
	r->accept( &v, IVisitable::RECURSE_DEPTH_FIRST );
	r->accept( &v, IVisitable::RECURSE_DEPTH_AFTER );
	}

	p = r->findByName("outer/inner/leaf");
	printf("root  use-count: %li, origin %s\n", r.use_count(), p->origin()->getName());
	p->dump( stdout ); fputc('\n', stdout);
	p->up();
	p->dump( stdout ); fputc('\n', stdout);
	Path p1 = p->findByName("leaf1[3]");
	printf("root  use-count: %li\n", r.use_count());
	p1->dump( stdout ); fputc('\n', stdout);

	printf("resetting\n");

	recurse( r, 0 );

	r.reset();
	p.reset();
	printf("root  use-count: %li\n", r.use_count());
	p1.reset();
	printf("root  use-count: %li\n", r.use_count());
	printf("outer use-count: %li\n", c1.use_count());


	// testing element counts
	Dev a = IDev::create("a");
	Dev b = IDev::create("b");
	Dev c = IDev::create("c");
	Dev d = IDev::create("d");
	Field l = IField::create("f");

	a->addAtAddress( b, 3  );
	b->addAtAddress( c, 5  );
	c->addAtAddress( d, 7  );
	d->addAtAddress( l, 11 );

	// single path
	p1 = a->findByName("b/c/d/f");
	if ( p1->getNelms() != 11*7*5*3 ) {
		throw TestFailed();
	}
	p1->up();
	p1->up();
	if ( p1->getNelms() != 5*3 ) {
		throw TestFailed();
	}
	p = c->findByName("d[4-5]/f[10]");
	p1->append(p);
	if ( p1->getNelms() != 2*5*3 ) {
		throw TestFailed();
	}
	Path p2 = a->findByName("b/c");
	if ( p2->getNelms() != 3*5 ) {
		throw TestFailed();
	}
	p1 = p2->concat( p );
	if ( p2->getNelms() !=  3*5 || p->getNelms() != 2 || p1->getNelms() !=  3*5*2 ) {
		p2->dump( stdout );
		p->dump( stdout );
		p1->dump( stdout );
		throw TestFailed();
	}

	CompositePathIterator it( &p1 );
	if ( it.getNelmsRight() != 1 || it.getNelmsLeft() != 3*5*2 ) {
		throw TestFailed();
	}

	Path p3 = d->findByName("f");

	++it;
	if ( it.getNelmsRight() != 1 || it.getNelmsLeft() != 3*5*2 ) {
		it.dump( stdout );
		throw TestFailed();
	}
	it.append(p3);
	if ( it.getNelmsRight() != 1 || it.getNelmsLeft() != 3*5*2*11 ) {
		it.dump( stdout );
		throw TestFailed();
	}
	++it;
	if ( it.getNelmsRight() != 11 || it.getNelmsLeft() != 3*5*2 ) {
		it.dump( stdout );
		throw TestFailed();
	}
	++it;
	if ( it.getNelmsRight() != 11*2 || it.getNelmsLeft() != 3*5 ) {
		it.dump( stdout );
		throw TestFailed();
	}
	++it;
	if ( it.getNelmsRight() != 11*2*5 || it.getNelmsLeft() != 3 ) {
		it.dump( stdout );
		throw TestFailed();
	}
	++it;
	if ( it.getNelmsRight() != 11*2*5*3 || it.getNelmsLeft() != 1 ) {
		it.dump( stdout );
		throw TestFailed();
	}

	p1 = a->findByName("b");
	p2 = b->findByName("c");
	p3 = c->findByName("d");


	Path els[] = {p3, p2, p1, Path((IPath *)0)};
	it = CompositePathIterator(&p1,&p2,&p3,0);
	unsigned nleft  = 1;
	unsigned nright = 1;
	for ( int i=0; els[i]; i++ ) {
		nleft *= els[i]->getNelms();
	}
	for ( int i=0; els[i]; i++ ) {
		if ( it.getNelmsRight() != nright || it.getNelmsLeft() != nleft ) {
			it.dump( stdout );
			throw TestFailed();
		}
		++it;
		nleft/=els[i]->getNelms();
		nright*= els[i]->getNelms();
	}

	printf("leaving\n");
} catch (CPSWError e ) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}
	if ( CpswObjCounter::report() ) {
		fprintf(stderr,"FAILED -- objects leaked\n");
		throw TestFailed();
	}

}
