#ifndef CPSW_DBG_OBJ_COUNT_H
#define CPSW_DBG_OBJ_COUNT_H

// count objects (for debugging)

#include <boost/atomic.hpp>

using boost::atomic;
using boost::memory_order_relaxed;

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

	static unsigned report(bool verbose=false);
};

#define DECLARE_OBJ_COUNTER(name,module,static_objs) \
CpswObjCounter & name() \
{ \
  static CpswObjCounter *xxx_ = new CpswObjCounter( module, static_objs ); \
  return *xxx_; \
}
#endif
