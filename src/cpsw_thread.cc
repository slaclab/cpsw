 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_thread.h>
#include <cpsw_error.h>

#include <stdio.h>

using std::string;

void * CRunnable::wrapper(void *arg)
{
CRunnable *me = static_cast<CRunnable*>(arg);
void     *rval;

fprintf(stderr,"Starting up '%s'\n", me->getName().c_str());
	try {
		rval = me->threadBody();
	} catch ( CPSWError e ) {
		fprintf(stderr,"Thread '%s' -- CPSW Error: %s\n", me->getName().c_str(), e.getInfo().c_str());
		throw;
	}

	return rval;
}

void CRunnable::threadStart()
{
int err;
	if ( ! started_ ) {
		if ( (err = pthread_create( &tid_, NULL, wrapper, this )) ) {
			throw InternalError("ERROR -- pthread_create", err);
		}
		started_ = true;
	}
}

void * CRunnable::threadJoin()
{
void *rval;
int   err;
	if ( (err = pthread_join( tid_, &rval )) ) {
		throw InternalError("ERROR -- pthread_join", err);
	}
	started_ = false;
	return rval;
}

void CRunnable::threadCancel()
{
int err;
	if ( started_ && (err = pthread_cancel( tid_ )) ) {
		throw InternalError("ERROR -- pthread_cancel", err);
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

//NOTE: the most derived class should call 'stop'
CRunnable::~CRunnable()
{
	threadStop();
}