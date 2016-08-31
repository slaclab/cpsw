#ifndef CPSW_MUTEX_HELPER_H
#define CPSW_MUTEX_HELPER_H

#include <cpsw_error.h>
#include <pthread.h>
#include <errno.h>
#include <boost/atomic.hpp>

class CMtx {
	pthread_mutex_t m_;
	const char     *nam_;

private:
	CMtx(const CMtx &);
	CMtx & operator=(const CMtx&);

public:

	class AttrRecursive {
	private:
		pthread_mutexattr_t a_;
		AttrRecursive(const AttrRecursive&);
		AttrRecursive operator=(const AttrRecursive&);
	public:
		AttrRecursive()
		{
		int err;
			if ( (err = pthread_mutexattr_init( &a_ )) ) {
				throw InternalError("pthread_mutexattr_init failed", err);
			}
			if ( (err = pthread_mutexattr_settype( &a_, PTHREAD_MUTEX_RECURSIVE )) ) {
				throw InternalError("pthread_mutexattr_settype(RECURSIVE) failed", err);
			}
		}

		~AttrRecursive()
		{
			pthread_mutexattr_destroy( &a_ );
		}

		const pthread_mutexattr_t *getp() const
		{
			return &a_;
		}
	};

	class MutexBusy {};

	class lg {
		private:
			CMtx *mr_;

			lg operator=(const lg&);

			lg(const lg&);

		public:
			lg(CMtx *mr, bool block=true)
			: mr_(mr)
			{
				if ( block ) {
					mr_->l();
				} else if ( ! mr_->t() ) {
					throw MutexBusy();
				}
			}

			~lg()
			{
				mr_->u();
			}

		friend class CMtx;
	};


	CMtx(const char *nam="<none>")
	: nam_(nam)
	{
	int err;
		if ( (err = pthread_mutex_init( &m_, NULL )) ) {
			throw InternalError("Unable to create mutex", err);
		}
	}

	CMtx( const AttrRecursive & attr, const char *nam = "<none>" )
	: nam_(nam)
	{
	int err;
		if ( (err = pthread_mutex_init( &m_, attr.getp() )) ) {
			throw InternalError("Unable to create mutex", err);
		}
	}

	~CMtx()
	{
		pthread_mutex_destroy( &m_ );
	}

	void l()
	{
	int err;
		if ( (err = pthread_mutex_lock( &m_ )) ) {
			throw InternalError("Unable to lock mutex", err);
		}
	}

	void u()
	{
	int err;
		if ( (err = pthread_mutex_unlock( &m_ )) ) {
			throw InternalError("Unable to unlock mutex", err);
		}
	}

	bool t()
	{
	int err;
		if ( (err = pthread_mutex_trylock( &m_ )) ) {
			if ( EBUSY == err )
				return false;
			throw InternalError("Unable to unlock mutex", err);
		}
		return true;
	}

	pthread_mutex_t *getp() { return &m_; }
};

// mutex with lazy initialization
class CMtxLazy {
private:
	boost::atomic<CMtx *> theMtx_;
	bool                  recursive_;
    const char           *name_;

private:
	CMtxLazy(const CMtxLazy &);
	CMtxLazy & operator=(const CMtxLazy &);

public:

	CMtxLazy(bool recursive = false, const char *name = "<none>")
	: recursive_(recursive),
	  name_(name)
	{
		theMtx_.store( 0, boost::memory_order_release );
	}

	CMtx * getMtx()
	{
		CMtx *mtx = theMtx_.load( boost::memory_order_acquire );
		if ( ! mtx ) {
			CMtx *nmtx = recursive_ ? new CMtx( CMtx::AttrRecursive(), name_ ) : new CMtx( name_ );
			do {
				if ( theMtx_.compare_exchange_weak( mtx,  nmtx, boost::memory_order_acq_rel ) )
					// successfully exchanged; 'nmtx' installed
					return nmtx;
			} while ( ! mtx );
            // if we get here then exchange was not performed AND mtx installed by someone else
			delete nmtx;
		}
		return mtx;
	}

	CMtx & operator*()
	{
		return *getMtx();
	}
	
	// Nobody must hold the mutex when it is being destroyed!
	~CMtxLazy()
	{
		CMtx *mtx = theMtx_.load( boost::memory_order_acquire );
		if ( mtx )
			delete( mtx );
	}

	class lg : public CMtx::lg {
		public:
			lg(CMtxLazy &m, bool block = true)
			: CMtx::lg( m.getMtx(), block )
			{
			}
	};
};

#endif
