#include <cpsw_obj_cnt.h>
#include <cpsw_shared_obj.h>
#include <stdio.h>

CpswObjCounter::CpswObjCounter(const char *name, unsigned statics)
: cnt_(0),
  statics_(statics),
  name_(name)
{
	next_ = lh(this);
}

CpswObjCounter *CpswObjCounter::lh(CpswObjCounter *o)
{
static LH lh;
	return o ? lh.add(o) : lh.get();
}
	

unsigned CpswObjCounter::getSum()
{
	unsigned rval = 0;
	CpswObjCounter *c;
	for (c = lh(0); c; c = c->next_) {
		rval += abs(c->get() - c->getStatics());
	}
	return rval;
}

unsigned CpswObjCounter::report(bool verbose)
{
unsigned rval = 0;
	CpswObjCounter *c;
	for (c = lh(0); c; c = c->next_) {
		unsigned d;
		if ( (d = abs(c->get() - c->getStatics()) ) || verbose ) {
			printf("Module '%s': remaining objects: %u, statics %u\n", c->getName(), c->get(), c->getStatics());
			rval += d;
		}
	}
	printf("Object Counter: Total %u obj leaked\n", rval);
	return rval;
}

DECLARE_OBJ_COUNTER( CShObj::sh_ocnt_, "ShObj", 1)
