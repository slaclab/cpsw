#include <cpsw_rssi_timer.h>

RssiTimer::RssiTimer( const char *name, RssiTimerList *lh, uint64_t exp)
: next_( NULL ),
  prev_( NULL ),
  lh_  (   lh ),
  exp_ (  exp ),
  name_( name )
{
}
