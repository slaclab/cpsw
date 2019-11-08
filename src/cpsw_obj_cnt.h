 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_DBG_OBJ_COUNT_H
#define CPSW_DBG_OBJ_COUNT_H

// count objects (for debugging)

#include <cpsw_compat.h>
#include <stdio.h>

using cpsw::atomic;
using cpsw::memory_order_relaxed;

class CpswObjCounter {
private:
	atomic<unsigned> cnt_;
	unsigned         statics_;
	const char      *name_;
	CpswObjCounter  *next_;

	class LH {
	private:
		CpswObjCounter *lh_;
	public:
		LH()
		: lh_(0)
		{
		}

		CpswObjCounter *add(CpswObjCounter *o)
		{
		CpswObjCounter *rval = lh_;
			lh_ = o;
			return rval;
		}

		CpswObjCounter *get()
		{
			return lh_;
		}
	};

private:
	CpswObjCounter(const CpswObjCounter&orig);
	CpswObjCounter & operator=(const CpswObjCounter&orig);

	static CpswObjCounter *lh(CpswObjCounter *o = NULL);

public:
	CpswObjCounter(const char *, unsigned statics = 0);

	CpswObjCounter &operator++()
	{
		cnt_.fetch_add(1, memory_order_relaxed);
		return *this;
	}

	CpswObjCounter &operator--()
	{
		cnt_.fetch_sub(1, memory_order_relaxed);
		return *this;
	}

	void addStatics(unsigned i)
	{
		statics_ += i;
	}

	unsigned get()
	{
		return cnt_.load( memory_order_relaxed );
	}

	unsigned getStatics()
	{
		return statics_;
	}

	const char *getName()
	{
		return name_;
	}


	static unsigned getSum();

	static unsigned report(FILE *f, bool verbose=false);
};

#define DECLARE_OBJ_COUNTER(name,module,static_objs) \
CpswObjCounter & name() \
{ \
	static CpswObjCounter *xxx_ = new CpswObjCounter( module, static_objs ); \
	return *xxx_; \
}
#endif
