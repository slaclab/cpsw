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
#include <cpsw_obj_cnt.h>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <stdio.h>

#include <cpsw_yaml.h>

struct TestFailed {};

class ExploreVisitor : public IPathVisitor {
private:
	typedef std::pair<std::string, unsigned> P;
	typedef std::vector<P>                   V;

	V           v_;
	V::iterator it_;
	V::iterator en_;
public:
	ExploreVisitor()
	{
	}

	void push_back(const char *s, unsigned n )
	{
		v_.push_back( P(s,n) );
	}

	void test(Path p)
	{
		it_ = v_.begin();
		en_ = v_.end();
		p->explore( this );
	}

	bool visitPre(ConstPath p)
	{
		if ( it_ == en_ ) {
			fprintf(stderr,"Expected results exhausted (can't check %s)\n", p->toString().c_str());
			throw TestFailed();
		}
		if ( (*it_).first != p->toString() ) {
			fprintf(stderr,"Unexpected path during 'explore' -- got %s, expected %s\n", p->toString().c_str(), (*it_).first.c_str());
			throw TestFailed();
		}
		++it_;
		return true;
	}

	void visitPost(ConstPath p)
	{
	}

	~ExploreVisitor() throw()
	{
		if ( it_ != en_ ) {
			fprintf(stderr,"Not all expected results checked\n");
			throw TestFailed();
		}
	}
};

static void test_explore_R3_5_3_21_g6dabdad()
{
const char *yaml=
"#schemaversion 3.0.0\n"
"root:\n"
"  " YAML_KEY_class ": Dev\n"
"  " YAML_KEY_children ":\n"
"    device:\n"
"      " YAML_KEY_class ": Dev\n"
"      " YAML_KEY_size ": 100\n"
"      " YAML_KEY_at ":\n"
"        " YAML_KEY_nelms ": 4\n"
"      " YAML_KEY_children ":\n"
"        reg:\n"
"          " YAML_KEY_class ": Field\n"
"          " YAML_KEY_size ": 1\n"
"          " YAML_KEY_at ":\n"
"            " YAML_KEY_nelms ": 16\n";

Hub	root = IPath::loadYamlStream( yaml, "root" )->origin();

Path p;

{
ExploreVisitor v;

p = root->findByName("device/reg");
	v.push_back("/device[0-3]/reg[0-15]", 4*16);

	v.test( p );
}
{
ExploreVisitor v;
p = root->findByName("device[0-3]/reg[0-15]");
	v.push_back("/device[0-3]/reg[0-15]", 4*16);

	v.test( p );
}
{
ExploreVisitor v;
p = root->findByName("device[2]/reg[3-4]");

	v.push_back("/device[2]/reg[3-4]", 2);

	v.test( p );
}
{
ExploreVisitor v;
p = root->findByName("device/reg[0]");
	v.push_back("/device[0-3]/reg[0]", 4);

	v.test( p );
}

{
ExploreVisitor v;
p = root->findByName("");
	v.push_back("/device[0]", 16);
	v.push_back("/device[0]/reg[0-15]", 16);
	v.push_back("/device[1]", 16);
	v.push_back("/device[1]/reg[0-15]", 16);
	v.push_back("/device[2]", 16);
	v.push_back("/device[2]/reg[0-15]", 16);
	v.push_back("/device[3]", 16);
	v.push_back("/device[3]/reg[0-15]", 16);

	v.test( p );
}

{
ExploreVisitor v;
p = root->findByName("device[1-2]");
	v.push_back("/device[1]",16);
	v.push_back("/device[1]/reg[0-15]",16);
	v.push_back("/device[2]",16);
	v.push_back("/device[2]/reg[0-15]",16);

	v.test( p );
}

}

static void test_a53564754e5eaa9029ff(bool use_yaml)
{
const char *yaml=
"#schemaversion 3.0.0\n"
"root:\n"
"  " YAML_KEY_class ": Dev\n"
"  " YAML_KEY_children ":\n"
"    device:\n"
"      " YAML_KEY_class ": Dev\n"
"      " YAML_KEY_size ": 100\n"
"      " YAML_KEY_at ":\n"
"        " YAML_KEY_nelms ": 4\n"
"      " YAML_KEY_children ":\n"
"        reg:\n"
"          " YAML_KEY_class ": Field\n"
"          " YAML_KEY_size ": 1\n"
"          " YAML_KEY_at ":\n"
"            " YAML_KEY_nelms ": 16\n";

Hub root;

if ( use_yaml ) {
	do {
	root = IPath::loadYamlStream( yaml, "root" )->origin();
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

static void test_dotdot_across_root_f8560f57884( Hub r )
{
	r->findByName("outer/../outer/inner");
}

using std::cout;

class DumpNameVisitor : public IPathVisitor {
private:
	bool pre_;
	int  indent_;
public:
	DumpNameVisitor() : pre_(false), indent_(0) {}

	void setPre(bool pre) { pre_ = pre; }

	virtual void pri(ConstPath p)
	{
	unsigned f = p->getTailFrom();
	unsigned t = p->getTailTo();
	int i;
		printf("E:");
		for ( i=0; i<indent_; i++ )
			printf(" ");
		printf("%s[%d", p->tail()->getName(), f);
		if ( f != t )
			printf("-%d",t);
		printf("]\n");
	}

	virtual bool visitPre(ConstPath p)
	{
		indent_++;
		if ( pre_ )
			pri( p );
		return true;
	}

	virtual void visitPost(ConstPath p)
	{
		if ( !pre_ )
			pri( p );
		indent_--;
	}
};

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
"#schemaversion 3.0.0\n"
"root:\n"
"  " YAML_KEY_class ": Dev\n"
"  " YAML_KEY_children ":\n"
"    outer:\n"
"      " YAML_KEY_class ": Dev\n"
"      " YAML_KEY_at ":\n"
"        " YAML_KEY_nelms ": 2\n"
"      " YAML_KEY_children ":\n"
"        inner:\n"
"          " YAML_KEY_class ": Dev\n"
"          " YAML_KEY_cacheable ": WT_CACHEABLE\n"
"          " YAML_KEY_at ":\n"
"            " YAML_KEY_nelms ": 4\n"
"          " YAML_KEY_children ":\n"
"            leaf:\n"
"              " YAML_KEY_class ": Field\n"
"              " YAML_KEY_size ": 7\n"
"              " YAML_KEY_at ":\n"
"                " YAML_KEY_nelms ": 1\n"
"            leaf1:\n"
"              " YAML_KEY_class ": Field\n"
"              " YAML_KEY_size ": 8\n"
"              " YAML_KEY_at ":\n"
"                " YAML_KEY_nelms ": 4\n";
	return IPath::loadYamlStream( yaml, "root" )->origin();
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

/*
try
*/
	{
Path    p  = IPath::create();
Hub     r  = use_yaml ? build_yaml() : build();

	printf("root  use-count: %li\n", r.use_count());

	{
	DumpNameVisitor v;
	p = IPath::create(r);
	v.setPre( false );
	p->explore( &v );
	v.setPre( true );
	p->explore( &v );
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

{
	IYamlSupport::dumpYamlFile( r, 0, 0 );
}

	{
		Path pa = r->findByName("outer[0]/inner/leaf1");
		Path pb = r->findByName("outer[1]/inner/leaf1");
		Path pi;
		if ( pa->intersect(pb) )
			throw TestFailed();
		pa = r->findByName("outer/inner[0-1]/leaf1");
		pb = r->findByName("outer/inner[2-3]/leaf1");
		if ( pa->intersect(pb) )
			throw TestFailed();
		pb = r->findByName("outer/inner/leaf1[1-3]");
		pa = r->findByName("outer/inner/leaf1[0]");
		if ( pa->intersect(pb) )
			throw TestFailed();

		pb = r->findByName("outer[0]/inner[0-1]/leaf1");
		pa = r->findByName("outer/inner[1-3]/leaf1[0]");

		if ( ! (pi = pa->intersect(pb)) || pi->getNelms() != 1 )
			throw TestFailed();

		pb = r->findByName("outer/inner/leaf1");
		pa = r->findByName("outer/inner[1-2]/leaf1");

		if ( ! (pi = pa->intersect(pb)) || pi->getNelms() != 16 )
			throw TestFailed();

		pb = r->findByName("outer/inner[0-2]/leaf1[1-3]");
		pa = r->findByName("outer/inner[1-3]/leaf1[0-2]");

		{
		Path pu = pa->clone();
		fprintf(stderr,"Checking parent of %s\n", pu->toString().c_str());
		if ( strcmp( pu->parent()->getName(), "inner" ) ) {
			fprintf(stderr,"Checking parent of %s FAILED\n", pu->toString().c_str());
			throw TestFailed();
		}
		pu->up();
		fprintf(stderr,"Checking parent of %s\n", pu->toString().c_str());
		if ( strcmp( pu->parent()->getName(), "outer" ) ) {
			fprintf(stderr,"Checking parent of %s FAILED\n", pu->toString().c_str());
			throw TestFailed();
		}
		pu->up();
		fprintf(stderr,"Checking parent of %s\n", pu->toString().c_str());
		if ( pu->parent() ) {
			// we are at the root
			throw TestFailed();
		}

		}

		if ( ! (pi = pa->intersect(pb)) || pi->getNelms() != 8 ) {
			throw TestFailed();
		}
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

	CompositePathIterator it( p1 );
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


	ConstPath els[] = {p3, p2, p1, Path((IPath *)0)};
	it = CompositePathIterator(p1).append(p2).append(p3);
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

	test_explore_R3_5_3_21_g6dabdad();

	test_dotdot_across_root_f8560f57884( build_yaml() );

	printf("leaving\n");
/*
} catch (CPSWError e ) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
*/
}
	if ( CpswObjCounter::report() ) {
		fprintf(stderr,"FAILED -- objects leaked\n");
		throw TestFailed();
	}

}
