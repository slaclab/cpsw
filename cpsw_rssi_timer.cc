 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_rssi_timer.h>

RssiTimer::RssiTimer( const char *name, RssiTimerList *lh, uint64_t exp)
: next_( NULL ),
  prev_( NULL ),
  lh_  (   lh ),
  exp_ (  exp ),
  name_( name )
{
}
