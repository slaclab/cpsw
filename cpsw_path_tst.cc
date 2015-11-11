#include <api_builder.h>
#include <stdio.h>

class DVisitor : public IVisitor {
	public:
	virtual void visit(Field f) { printf("Visiting default E (%s)\n", f->getName()); }
	virtual void visit(Dev   d) { printf("Visiting default D (%s)\n", d->getName()); }
};

class DumpNameVisitor : public IVisitor {
public:
	virtual void visit(Field e) { printf("E: %s\n", e->getName()); }
}; 

int
main(int argc, char **argv)
{
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
	r.reset();
	p.reset();
	printf("root  use-count: %li\n", r.use_count());
	p1.reset();
	printf("root  use-count: %li\n", r.use_count());
	printf("outer use-count: %li\n", c1.use_count());
	printf("leaving\n");
}
