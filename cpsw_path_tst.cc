#include <cpsw_api_builder.h>
#include <cpsw_path.h>
#include <cpsw_obj_cnt.h>
#include <sstream>
#include <string>
#include <iostream>
#include <stdio.h>

static void test_a53564754e5eaa9029ff(bool use_yaml)
{
const char *yaml=
"name: root\n"
"class: Dev\n"
"children:\n"
"- name: device\n"
"  class: Dev\n"
"  size: 100\n"
"  nelms: 4\n"
"  children:\n"
"  - name: reg\n"
"    class: Field\n"
"    size: 1\n"
"    nelms: 16\n";

Hub root;

if ( use_yaml ) {
	do {
	root = IHub::loadYamlStream( yaml, 0 );
	} while (0);
} else {

	Dev rdev = IDev::create("root");
	Dev dev  = IDev::create("device", 100);
	Field f  = IField::create("reg", 1);
	dev->addAtAddress( f, 16 );
	rdev->addAtAddress( dev, 4 );
	root = rdev;
}

Path p1 = root->findByName("device/reg");
Path p2 = root->findByName("device[0-3]/reg[0-15]");
Path p3 = root->findByName("device[2]/reg[3-4]");
Path p4 = root->findByName("device/reg[0]");
	printf("%d\n", p1->getNelms());
	printf("%d\n", p2->getNelms());
	printf("%d\n", p3->getNelms());
	printf("%d\n", p4->tail()->getNelms());
}

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

static Hub build_yaml()
{
const char *yaml=
"name: root\n"
"class: Dev\n"
"children:\n"
"- name: outer\n"
"  class: Dev\n"
"  nelms: 2\n"
"  children:\n"
"  - name: inner\n"
"    class: Dev\n"
"    cacheable: WT_CACHEABLE\n"
"    nelms: 4\n"
"    children:\n"
"    - name: leaf\n"
"      class: Field\n"
"      size: 7\n"
"    - name: leaf1\n"
"      class: Field\n"
"      size: 8\n"
"      nelms: 4\n";
	return IHub::loadYamlStream( yaml, 0 );
}

static Dev build()
{
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

	return r;
}


int
main(int argc, char **argv)
{
bool use_yaml = false;
int  opt;

while ( (opt = getopt(argc, argv, "Y")) > 0 ) {
	switch (opt) {
		default:
			fprintf(stderr,"Unknown option -%c\n", opt);
			exit(1);
		case 'Y':
			use_yaml = true;
		break;
	}
}

try {
Path    p  = IPath::create();
Hub     r  = use_yaml ? build_yaml() : build();

	printf("root  use-count: %li\n", r.use_count());
#if 0
	{
	DumpNameVisitor v;
	r->accept( &v, IVisitable::RECURSE_DEPTH_FIRST );
	r->accept( &v, IVisitable::RECURSE_DEPTH_AFTER );
	}
#endif

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

{
	IYamlSupport::dumpYamlFile( r, 0, 0 );
}

	r.reset();
	p.reset();
	printf("root  use-count: %li\n", r.use_count());
	p1.reset();
	printf("root  use-count: %li\n", r.use_count());


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

	p1 = a->findByName("b/c/d/../../c/d/f/..");
	if ( p1->getNelms() != 3*5*7 ) {
		p1->dump(stdout);
		throw TestFailed();
	}

	test_a53564754e5eaa9029ff(use_yaml);


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
