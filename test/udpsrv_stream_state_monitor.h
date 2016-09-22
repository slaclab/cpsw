#ifndef UDPSRV_STREAM_STATE_MONITOR_H
#define UDPSRV_STREAM_STATE_MONITOR_H
 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_event.h>
#include <cpsw_thread.h>

class StreamStateMonitor : public CRunnable {
private:
	EventSet eventSet_;
	StreamStateMonitor();
public:

	EventSet getEventSet()
	{
		return eventSet_;
	}


	virtual void *threadBody();

	static StreamStateMonitor *getTheMonitor();
};

#endif
