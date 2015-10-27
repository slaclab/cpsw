#include <api_user.h>
#include <cpsw_hub.h>

int
main(int argc, char **argv)
{
Path p = PathInterface::create();
Dev  r("ROOT");
Dev  c1("outer");
Dev  c2("inner");
Dev  c3("leaf");
Dev  c4("leaf1");
	c2.addAtAddr(&c3);
	c2.addAtAddr(&c4, 4);
	c1.addAtAddr(&c2, 4);
	r.addAtAddr(&c1,2);
	p = r.findByName("outer/inner/leaf");
	p->dump( stdout ); fputc('\n', stdout);
	p->up();
	p->dump( stdout ); fputc('\n', stdout);
	Path p1 = p->findByName("leaf1[3]");
	p1->dump( stdout ); fputc('\n', stdout);
	
}
