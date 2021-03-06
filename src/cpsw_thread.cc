 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#if defined(__linux__) && ! defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <cpsw_thread.h>
#include <cpsw_error.h>
#include <cpsw_stdio.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
#include <errno.h>

#include <stdio.h>
#include <signal.h>

#define CPSW_THREAD_DEBUG 0

#ifdef CPSW_THREAD_DEBUG
int cpsw_thread_debug = CPSW_THREAD_DEBUG;
#endif

using std::string;

CRunnable::CRunnable(const char *name, int prio)
: started_(false),
  name_(name),
  prio_(prio)
{
}

CRunnable::CRunnable(const CRunnable &orig)
: started_(false),
  name_(orig.name_),
  prio_(orig.prio_)
{
}

CRunnable::Attr::Attr()
{
int err;
	if ( (err = pthread_attr_init( &a_ )) ) {
		throw InternalError("ERROR -- pthread_attr_init", err);
	}
}

CRunnable::Attr::~Attr() throw()
{
int err;
	if ( (err = pthread_attr_destroy( &a_ )) ) {
		throw InternalError("ERROR -- pthread_attr_destroy", err);
	}
}

// RAII for modifying and restoring signal mask
class CRunnable::SigMask {
private:
	sigset_t orig_;
	SigMask(const SigMask &);
	SigMask &operator=(const SigMask &);
public:
	SigMask();
	~SigMask() throw();
};

void * CRunnable::wrapper(void *arg)
{
CRunnable *me = static_cast<CRunnable*>(arg);
void     *rval;

#ifdef CPSW_THREAD_DEBUG
if ( cpsw_thread_debug > 0 ) {
	fprintf(CPSW::fDbg(), "Starting up thread '%s'", me->getName().c_str());
#ifdef __linux__
	{
		pid_t tid = syscall( SYS_gettid );
		fprintf(CPSW::fDbg(), " (LWP %ld)", (long)tid);
	}
#endif
	fprintf(CPSW::fDbg(), " priority %d\n", me->getPrio());
}
#endif

	try {
		rval = me->threadBody();
	} catch ( CPSWError &e ) {
		
		fprintf(CPSW::fErr(), "Thread '%s' -- CPSW Error: %s\n", me->getName().c_str(), e.getInfo().c_str());
		throw;
	}

	return rval;
}

void CRunnable::threadStart()
{
int err;
int attempts;
	for ( attempts = 2; attempts > 0 && ! started_; attempts-- ) {
		Attr    attr;
		SigMask blockAllSignals; // start new thread with all signals blocked

#if defined _POSIX_THREAD_PRIORITY_SCHEDULING
		int                pol   = prio_ > 0 ? SCHED_FIFO : SCHED_OTHER;
		struct sched_param param;

			pol                  = prio_ > 0 ? SCHED_FIFO : SCHED_OTHER;
			param.sched_priority = prio_ > 0 ? prio_ : 0;
		if ( (err = pthread_attr_setschedpolicy( attr.getp(), pol )) ) {
			throw InternalError("ERROR -- pthread_attr_setschedpolicy", err);
		}
		if ( SCHED_OTHER != pol ) {
			if ( (err = pthread_attr_setschedparam( attr.getp(), &param )) ) {
				throw InternalError("ERROR -- pthread_attr_setschedparam", err);
			}
		}
		if ( (err = pthread_attr_setinheritsched( attr.getp(), PTHREAD_EXPLICIT_SCHED)) ) {
			throw InternalError("ERROR -- pthread_attr_setschedpolicy", err);
		}
#else
		#warning "_POSIX_THREAD_PRIORITY_SCHEDULING not defined -- always using default priority"
		prio_ = 0;
#endif
		if ( (err = pthread_create( &tid_, attr.getp(), wrapper, this )) ) {
			if ( EPERM == err && prio_ > 0 ) {
				ErrnoError warn(getName(), err);
				fprintf(CPSW::fErr(), "WARNING: CRunnable::threadStart; unable to set priority for %s -- IGNORED\n", warn.what());
				// Try again with default priority
				prio_ = 0;
				continue;
			} else {
				throw InternalError("ERROR -- pthread_create()", err);
			}
		}
		started_ = true;
	}
}

void * CRunnable::threadJoin()
{
void *rval;
int   err;
	if ( (err = pthread_join( tid_, &rval )) ) {
		throw InternalError("ERROR -- pthread_join()", err);
	}
	started_ = false;
	return rval;
}

void CRunnable::threadCancel()
{
int err;
	if ( started_ && (err = pthread_cancel( tid_ )) ) {
		throw InternalError("ERROR -- pthread_cancel()", err);
	}
}

bool CRunnable::threadStop(void **r_p)
{
	if ( started_ ) {
		threadCancel();
		void *val = threadJoin();
		if (r_p)
			*r_p = val;
		return true;
	}
	return false;
}

int CRunnable::getPrio() const
{
#if defined _POSIX_THREAD_PRIORITY_SCHEDULING
	return prio_;
#else
	#warning "_POSIX_THREAD_PRIORITY_SCHEDULING not defined -- always using default priority"
	return 0;
#endif
}

int CRunnable::setPrio(int prio)
{
#if defined _POSIX_THREAD_PRIORITY_SCHEDULING
int                err;
struct sched_param param;
int                pol;

	if ( started_ ) {
		param.sched_priority = prio > 0 ? prio       : 0;
		pol                  = prio > 0 ? SCHED_FIFO : SCHED_OTHER;
		if ( (err = pthread_setschedparam( tid_, pol, &param )) ) {
			if ( EPERM == err ) {
				ErrnoError warn(getName(), err);
				fprintf(CPSW::fErr(), "WARNING: CRunnable::setPriority failed for %s", warn.what());
				if ( (err = pthread_getschedparam( tid_, &pol, &param )) ) {
					throw InternalError("ERROR: pthread_getschedparam()", err);
				}
				prio = param.sched_priority;
			} else {
				throw InternalError("ERROR: pthread_setschedparam()", err);
			}
		}
	}
	prio_ = prio;
	return prio_;
#else
	#warning "_POSIX_THREAD_PRIORITY_SCHEDULING not defined -- always using default priority"
	return 0;
#endif
}

//NOTE: the most derived class should call 'stop'
CRunnable::~CRunnable()
{
	threadStop();
}

CRunnable::SigMask::SigMask()
{
int err;
sigset_t all;
	sigfillset( &all );
	if ( (err = pthread_sigmask( SIG_SETMASK, &all, &orig_ )) ) {
		throw InternalError("pthread_sigmask (block all)", err);
	}
}

CRunnable::SigMask::~SigMask() throw()
{
int err;
	if ( (err = pthread_sigmask( SIG_SETMASK, &orig_, NULL )) ) {
		throw InternalError("pthread_sigmask (restore)", err);
	}
}
