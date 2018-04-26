 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_RSSI_TIMER_H
#define CPSW_RSSI_TIMER_H

#include <cpsw_error.h>
#ifdef NO_CPSW
#include <to.h>
#else
#include <cpsw_api_timeout.h>
#endif

#include <stdint.h>
#include <stdio.h>
class RssiTimerList;

class RssiTimer {
private:
	RssiTimer       *next_;
	RssiTimer       *prev_;
	RssiTimer       *lh_;

	CTimeout         exp_;

	const char *     name_;

protected:

	void addAfter(RssiTimer *el)
	{
		if ( (next_ = el->next_) ) {
			next_->prev_ = this;
		}
		el->next_ = this;
		prev_ = el;
	}

	RssiTimer      *getPrev()
	{
		return prev_;
	}

	RssiTimer      *getNext()
	{
		return next_;
	}


	// subclass should implement a useful 'process()'
	RssiTimer( const char *name, RssiTimerList *lh, uint64_t exp = 0 );

public:
	const char *getName()
	{
		return name_;
	}

	RssiTimer *lh()
	{
		return lh_;
	}

	void dumpList(FILE *f)
	{
	RssiTimer *t = lh();
		while ( t ) {
			fprintf(f, "%s -> ", t->getName());
			t = t->next_;
		}
		fprintf(f, "\n");
	}


	void cancel()
	{
		if ( next_ )
			next_->prev_ = prev_;
		if ( prev_ )
			prev_->next_ = next_;
		next_ = prev_ = NULL;
	}

	void arm_abs(const CTimeout exp)
	{
	RssiTimer *el;

		cancel();

		for ( el = lh(); el; el=el->next_ ) {
			if ( ! el->next_ || exp < el->next_->exp_ ) {
				addAfter( el );
				exp_ = exp;
				return;
			}
		}
	}

	void arm_rel(CTimeout &exp)
	{
	CTimeout abst;
		// FIXME -- should use method associated with synchronization device
		if ( clock_gettime( CLOCK_REALTIME, &abst.tv_ ) ) {
			throw InternalError("glock_gettime failed");
		}
		arm_abs( abst += exp );
	}

	bool isArmed()
	{
		return !!prev_;
	}

	const CTimeout *getTimeout()
	{
		return &exp_;
	}

	virtual ~RssiTimer()
	{
		cancel();
	}

	virtual void process()
	{
	}
};

class RssiTimerList : public RssiTimer {
public:
	RssiTimerList()
	: RssiTimer( "LH", this, 0 )
	{
	}

	RssiTimer *getFirstToExpire()
	{
		return getNext();
	}
};

#endif
