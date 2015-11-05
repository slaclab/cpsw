#include <api_user.h>
#include <cpsw_hub.h>

class DVisitor : public Visitor {
	public:
	virtual void visit(const Entry *e) { printf("Visiting default E\n"); }
	virtual void visit(const Dev   *e) { printf("Visiting default D\n"); }
	virtual void visit(const Visitable *e) { printf("Visiting default V\n"); }
};

class DumpNameVisitor : public Visitor {
public:
	virtual void visit(const Entry *e) { printf("E: %s\n", e->getName()); }
}; 

int
main(int argc, char **argv)
{
Path p = IPath::create();
Dev  r("ROOT");
Dev  c1("outer");
Dev  c2("inner");
Entry  c3("leaf", 7);
Entry  c4("leaf1", 8);
	c2.addAtAddr(&c3);
	c2.addAtAddr(&c4, 4);

	c2.setCacheable( true );
	c1.addAtAddr(&c2, 4);
	r.addAtAddr(&c1,2);
	p = r.findByName("outer/inner/leaf");
	p->dump( stdout ); fputc('\n', stdout);
	p->up();
	p->dump( stdout ); fputc('\n', stdout);
	Path p1 = p->findByName("leaf1[3]");
	p1->dump( stdout ); fputc('\n', stdout);

	{
	DumpNameVisitor v;
	r.accept( &v, true );
	r.accept( &v, false );
	}
	
}
