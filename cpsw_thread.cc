#include <cpsw_thread.h>
#include <cpsw_error.h>

void * Runnable::wrapper(void *arg)
{
Runnable *me = static_cast<Runnable*>(arg);

	return me->body();
}

void Runnable::start()
{
int err;
	if ( (err = pthread_create( &tid_, NULL, wrapper, this )) ) {
		throw InternalError("ERROR -- pthread_create", err);
	}
	started_ = true;
}

void * Runnable::join()
{
void *rval;
int   err;
	if ( (err = pthread_join( tid_, &rval )) ) {
		throw InternalError("ERROR -- pthread_join", err);
	}
	started_ = false;
	return rval;
}

void Runnable::cancel()
{
int err;
	if ( started_ && (err = pthread_cancel( tid_ )) ) {
		throw InternalError("ERROR -- pthread_cancel", err);
	}
}

bool Runnable::stop(void **r_p)
{
	if ( started_ ) {
		cancel();
		void *val = join();
		if (r_p)
			*r_p = val;
		return true;
	}
	return false;
}

//NOTE: the most derived class should call 'stop'
Runnable::~Runnable()
{
	stop();
}
