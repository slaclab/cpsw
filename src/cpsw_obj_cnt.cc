 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_obj_cnt.h>
#include <cpsw_shared_obj.h>
#include <stdio.h>
#include <cpsw_stdio.h>

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

unsigned CpswObjCounter::report(FILE *f, bool verbose)
{
unsigned rval = 0;
	CpswObjCounter *c;
	for (c = lh(0); c; c = c->next_) {
		unsigned d;
		if ( (d = abs(c->get() - c->getStatics()) ) || verbose ) {
			fprintf(f, "Module '%s': remaining objects: %u, statics %u\n", c->getName(), c->get(), c->getStatics());
			rval += d;
		}
	}
	fprintf(f, "Object Counter: Total %u obj leaked\n", rval);
	return rval;
}

DECLARE_OBJ_COUNTER( CShObj::sh_ocnt_, "ShObj", 1)
