#ifndef CPSW_SEM_H
#define CPSW_SEM_H

#include <semaphore.h>
#include <cpsw_error.h>

class CSem {
private:
	sem_t _sem;
public:
	CSem(unsigned int val = 0)
	{
		if ( sem_init( &_sem, 0, val ) ) {
			throw InternalError("Unable to initialize semaphore", errno);
		}
	}

	void post()
	{
		if ( sem_post( &_sem ) ) {
			throw InternalError("Posting semaphore failed", errno);
		}
	}

	void wait()
	{
		if ( sem_wait( &_sem ) ) {
			throw InternalError("Wait for semaphore failed", errno);
		}
	}

	~CSem()
	{
		if ( sem_destroy( &_sem ) ) {
			throw InternalError("Unable to destroy semaphore", errno);
		}
	}
};

#endif
