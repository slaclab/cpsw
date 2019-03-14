 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_QUEUE_FALLBACK_H
#define CPSW_QUEUE_FALLBACK_H

/* Trivial (non-lock free!) queue as a fallback when building w/o boost */

#include <cpsw_mutex.h>
#include <vector>

typedef std::size_t size_type;

class CBufQueueBase {
	std::vector<IBufChain*> fifo_;
	unsigned                rp_, wp_, fl_, sz_;
	CMtx                    m_;
public:
	CBufQueueBase(size_type n)
	: fifo_( n ),
	  rp_  ( 0 ),
	  wp_  ( 0 ),
	  fl_  ( 0 ),
	  sz_  ( n )
	{
	}
    bool pop(IBufChain* &p)
	{
	CMtx::lg guard( &m_ );
		if ( fl_ == 0 )
			return false;
		p = fifo_[rp_];
		if ( ++rp_ == sz_ )
			rp_ = 0;
		fl_--;
		return true;
	}

    bool bounded_push(IBufChain * const & p)
	{
	CMtx::lg guard( &m_ );
		if ( fl_ == sz_ )
			return false;
		fifo_[wp_] = p;
		if ( ++wp_ == sz_ )
			wp_ = 0;
		fl_++;
		return true;
	}
};

#endif
