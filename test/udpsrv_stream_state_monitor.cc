 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <udpsrv_stream_state_monitor.h>

StreamStateMonitor*
StreamStateMonitor::getTheMonitor()
{
static StreamStateMonitor *theMonitor = new StreamStateMonitor();
	return theMonitor;
}

StreamStateMonitor::StreamStateMonitor()
: CRunnable("StreamStateMonitor"),
  eventSet_( IEventSet::create() )
{
	threadStart();
}

void *
StreamStateMonitor::threadBody()
{
	while ( 1 ) {
		eventSet_->processEvent(true, 0);	
	}
	return 0;
}
