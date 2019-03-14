 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#ifndef CPSW_FREE_LIST_STACK_H
#define CPSW_FREE_LIST_STACK_H

/* A trivial (non-lock free!) free-list implementation which can be
 * used as a fallback if boost is not available.
 */

#include <cpsw_error.h>
#include <cpsw_mutex.h>

template <typename T, typename ALL>
class freelist_stack {
private:
	ALL  all_;
	T   *h_;
	CMtx m_;
public:
	freelist_stack(ALL all, int unused)
	: all_( all              ),
	  h_  ( NULL             ),
      m_  ( "freelist_stack" )
	{
		if ( sizeof(T) < sizeof( T* ) ) {
			throw InternalError("Freelist nodes too small");
		}
	}

	template <bool unused1, bool unused2> 
	T *allocate()
	{
	T *rval;
		{
		CMtx::lg guard( &m_ );
		if ( (rval = h_) ) {
			h_ = *reinterpret_cast<T**>(rval);
			return rval;
		} 
		}
		return all_.allocate(1);
	}

	template <bool unused>
	void deallocate(T *el)
	{
	CMtx::lg guard( &m_ );
		*reinterpret_cast<T**>(el) = h_;
		h_ = el;
	}
};

#endif
