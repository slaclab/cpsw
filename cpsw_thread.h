#ifndef CPSW_THREAD_HELPER_H
#define CPSW_THREAD_HELPER_H

#include <pthread.h>

class Runnable {
private:
	pthread_t tid_;	
	bool      started_;

	static void *wrapper(void*);

protected:
	// to be implemented by subclasses

	virtual void * body()    = 0; 

	pthread_t getTid() { return tid_ ; }


public:
	Runnable() : started_(false) {}

	virtual void start();
	virtual void * join();
	virtual void cancel();
	virtual bool stop(void **joinval_p);
	virtual bool stop() { return stop(NULL); }

	// Destructor of subclass should call stop -- cannot
	// rely on base class to do so because subclass 
	// data is already torn down!
	virtual ~Runnable();
};

#endif
