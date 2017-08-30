 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_THREAD_HELPER_H
#define CPSW_THREAD_HELPER_H

#include <pthread.h>
#include <string>

class CRunnable {
private:
	pthread_t     tid_;
	bool          started_;
	std::string   name_;
	int           prio_;

	static void*  wrapper(void*);

protected:
	// to be implemented by subclasses

	virtual void* threadBody()    = 0;

	pthread_t     getTid() { return tid_ ; }

	class Attr {
	private:
		Attr(const Attr&);
		Attr & operator=(const Attr &);
		pthread_attr_t a_;
	public:
		Attr();
		~Attr();
	
		pthread_attr_t *getp()
		{
			return &a_;
		}
	};


public:

	static const int DFLT_PRIORITY = 0;

	CRunnable(const char *name, int prio = DFLT_PRIORITY);

	CRunnable(const CRunnable &orig);

	const std::string& getName() { return name_; }

	virtual void  threadStart();
	virtual void* threadJoin();
	virtual void  threadCancel();

	// returns 'false' when the thread hadn't been
	// started yet.

	// note that the 'started_' flag is manipulated
	// by the 'start', 'stop' and 'join' methods
	// in the context of their caller.
	virtual bool threadStop(void **joinval_p);
	virtual bool threadStop() { return threadStop(NULL); }

	// Set pthread priority (<=0: default scheduler, >0 SCHED_FIFO priority)
	// A warning is printed (and the priority unchanged) if this call
	// fails due to lack of permission.
	// RETURNS: new priority (== old priority if there was no permission)
	virtual int  setPrio(int prio);
	virtual int  getPrio() const;

	// Destructor of subclass should call stop -- cannot
	// rely on base class to do so because subclass
	// data is already torn down!
	virtual ~CRunnable();
};

#endif
