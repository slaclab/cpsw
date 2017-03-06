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

	/* Proper handling of pthread_cond_wait being cancelled is a bit ugly.
	 * (Requires pthread_cleanup_push/pop which are macros and not real
	 * functions.
	 * Provide a helper which throws if pthread_mutex_unlock fails.
	 */
	static void pthread_mutex_unlock_wrapper( void *themutex )
	{
	int err;
		if ( (err = pthread_mutex_unlock( (pthread_mutex_t*) themutex )) ) {
			throw InternalError("CAsyncIOTransactionManager -- unable to unlock mutex", err);
		}

	}
};

#endif
