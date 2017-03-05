#ifndef CPSW_CONDVAR_H
#define CPSW_CONDVAR_H

#include <pthread.h>
#include <cpsw_mutex.h>

class CondInitFailed   {};
class CondWaitFailed   {};
class CondSignalFailed {};


/* RAII for pthread condition variables */
class CCond {
private:
	pthread_cond_t cond_;

	CCond(const CCond&);
	CCond & operator=(const CCond&);
public:
	CCond()
	{
		if ( pthread_cond_init( &cond_, NULL ) )
			throw CondInitFailed();
	}

	~CCond()
	{
		pthread_cond_destroy( &cond_ );
	}

	pthread_cond_t *getp() { return &cond_; }
};

#endif
