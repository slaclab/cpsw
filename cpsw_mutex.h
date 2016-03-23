#ifndef CPSW_MUTEX_HELPER_H
#define CPSW_MUTEX_HELPER_H

#include <cpsw_error.h>
#include <pthread.h>

class CMtx {
	pthread_mutex_t m_;
	const char     *nam_;

private:
	CMtx(const CMtx &);
	CMtx & operator=(const CMtx&);
public:

	class lg {
		private:
			CMtx *mr_;

			lg operator=(const lg&);

			lg(const lg&);

		public:
			lg(CMtx *mr)
			: mr_(mr)
			{
				mr_->l();
			}

			~lg()
			{
				mr_->u();
			}

		friend class CMtx;
	};


	CMtx(const char *nam):nam_(nam)
	{
	int err;
		if ( (err = pthread_mutex_init( &m_, NULL )) ) {
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

	pthread_mutex_t *getp() { return &m_; }
};

#endif
