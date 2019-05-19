 //@C Copyright Notice  
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_fs_addr.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>
#include <pthread.h>

//#define CFS_DEBUG

/* Using 'select' as a timeout mechanism for writes is not suitable because
 * the actual 'write' can still block. Also, e.g., uio(7) does not implement
 * EPOLLOUT.
 * An implementation would likely to have to use timers and signals in order
 * to interrupt a 'write' or resort so some asynchronous facility.
 */
#undef USE_SELECT_ON_WRITE

#define DFLT_SEEKABLE true
#define DFLT_OFFSET   ((uint64_t)0)

static void readTimeout (int fd, uint8_t *buf, unsigned n, const CTimeout *timeout);
static void writeTimeout(int fd, uint8_t *buf, unsigned n, const CTimeout *timeout);


CFSAddressImpl::CFSAddressImpl(AKey key, YamlState &ypath)
: CAddressImpl( key, ypath    ),
  fd_         ( -1            ),
  seekable_   ( DFLT_SEEKABLE ),
  offset_     ( DFLT_OFFSET   )
{
uint64_t us;

	mustReadNode( ypath, YAML_KEY_fileName, &fileName_ );
	readNode( ypath, YAML_KEY_seekable,  &seekable_ );
	readNode( ypath, YAML_KEY_offset,    &offset_    );
	if ( (hasTimeout_ = readNode( ypath, YAML_KEY_timeoutUS, &us )) ) {
		timeout_.set( us );
	}
}

void
CFSAddressImpl::dumpYamlPart(YAML::Node &node) const
{
	CAddressImpl::dumpYamlPart( node );
	writeNode( node, YAML_KEY_fileName, fileName_ );
	if ( seekable_ != DFLT_SEEKABLE ) {
		writeNode( node, YAML_KEY_seekable, seekable_ );
	}
	if ( offset_ != DFLT_OFFSET ) {
		writeNode( node, YAML_KEY_offset, offset_ );
	}
	if ( hasTimeout_ ) {
		writeNode( node, YAML_KEY_timeoutUS, timeout_.getUs() );
	}
}

int
CFSAddressImpl::open (CompositePathIterator *pi)
{
CMtx::lg guard( &mtx_ );

	{
	int rval = incOpen();

		if ( rval < 0 ) {
			throw InternalError("CFSAddressImpl::open negative open count??");
		}

		if ( 0 == rval ) {
#ifdef CFS_DEBUG
			fprintf(stderr,"CFSAddressImpl::open real open\n");
#endif
			fd_ = ::open( fileName_.c_str(), O_RDWR );
			if ( fd_ <  0 ) {
				std::string msg = std::string("Unable to open file: ") + fileName_;
				throw IOError( msg );
			}
		}

		return rval;
	}
}

int
CFSAddressImpl::close(CompositePathIterator *pi)
{
CMtx::lg guard( &mtx_ );


	{
	int rval = decOpen();

		if ( rval <= 0 ) {
			throw InternalError("CFSAddressImpl::close negative open count??");
		}

		if ( 1 == rval ) {
#ifdef CFS_DEBUG
			fprintf(stderr,"CFSAddressImpl::close real close\n");
#endif
			::close( fd_ );
			fd_ = -1;
		}

		return rval;
	}
}

CFSAddressImpl::~CFSAddressImpl()
{
	::close( fd_ );
}

struct ThreadArgs {
	int            fd_;
	uint8_t       *dst_;
	unsigned       nbytes_;
	const CTimeout timeout_;
	AsyncIO        aio_;

	ThreadArgs(const CTimeout *pto)
	: timeout_( *pto )
	{
	}
};

static void *
threadFunc(void *arg)
{
ThreadArgs *rarg = (ThreadArgs *)arg;

	try {
		readTimeout( rarg->fd_, rarg->dst_, rarg->nbytes_, &rarg->timeout_ );
		rarg->aio_->callback( 0 );
	} catch ( CPSWError &e ) {
		rarg->aio_->callback( &e );	
	} catch (...) {
		delete rarg;

		throw InternalError("Unknown exception in reader thread");
	}

	delete rarg;

	return 0;
}


uint64_t
CFSAddressImpl::read (CReadArgs  *args) const
{
const CTimeout *pto = getTimeout( 0 ) && args->timeout_.isIndefinite() ? &timeout_ : &args->timeout_;

	if ( isSeekable() && ((off_t)-1 == lseek( fd_, getOffset() + args->off_, SEEK_SET )) ) {
		throw IOError("CFSAddressImpl::read lseek failed");
	}

	if ( args->aio_ && ! pto->isNone() ) {
		pthread_t   tid;
		int         err;
		ThreadArgs *rargs = new ThreadArgs( pto );

		rargs->fd_      = fd_;
		rargs->dst_     = args->dst_;
		rargs->nbytes_  = args->nbytes_;
		rargs->aio_     = args->aio_;

		// FIXME: better implementation; this is not very efficient!

		if ( (err = pthread_create( &tid, 0, threadFunc, rargs )) ) {
			delete rargs;

			throw ErrnoError("CFSAddressImpl::read: unable to create thread", err);
		}

	} else {

		readTimeout( fd_, args->dst_, args->nbytes_, pto );

		if ( args->aio_ ) {
			args->aio_->callback( 0 );
		}

	}

	return args->nbytes_;
}

uint64_t
CFSAddressImpl::write(CWriteArgs *args) const
{
uint8_t   msk1 = args->msk1_;
uint8_t   mskn = args->mskn_;
uint8_t   *src = args->src_;
unsigned     n = args->nbytes_;
unsigned     m;
const CTimeout *pto = getTimeout( 0 ) && args->timeout_.isIndefinite() ? &timeout_ : &args->timeout_;

	if ( msk1 || mskn ) {
		if ( args->cacheable_ < IField::WT_CACHEABLE ) {
			throw InvalidArgError("CFSAddressImpl::write Cannot merge bits when writing to non-cacheable area");
		}
		if ( ! isSeekable() ) {
			throw InvalidArgError("CFSAddressImpl::write Cannot merge bits when writing to non-seekable area");
		}
	}

	if ( isSeekable() && ((off_t)-1 == lseek( fd_, getOffset() + args->off_, SEEK_SET )) ) {
		throw IOError("CFSAddressImpl::write lseek failed");
	}

	if ( n == 1 ) {
		msk1 |= mskn;
		mskn  = 0;
	}

	if ( msk1 ) {
		uint8_t   buf;

		/* FIXME: should convert to absolute timeout */
		readTimeout( fd_, &buf, 1, pto );

		if ( (off_t)-1 == lseek( fd_, (off_t)-1, SEEK_CUR ) ) {
			throw IOError("CFSAddressImpl::write lseek failed (reposition for merging)");
		}
		
		buf = (*src & ~msk1) | ( buf & msk1 );

		writeTimeout( fd_, &buf, 1, pto );

		src++;
		n--;
	}

	m = mskn ? n - 1 : n;

	writeTimeout( fd_, src, m, pto );

	if ( mskn ) {
		uint8_t   buf;

		src += m;

		readTimeout( fd_, &buf, 1, pto );

		if ( (off_t)-1 == lseek( fd_, (off_t)-1, SEEK_CUR ) ) {
			throw IOError("CFSAddressImpl::write lseek failed (reposition for merging)");
		}
		
		buf = (*src & ~mskn) | ( buf & mskn );

		writeTimeout( fd_, &buf, 1, pto );

	}

	return args->nbytes_;
}

static void readTimeout (int fd, uint8_t *buf, unsigned n, const CTimeout *timeout)
{
fd_set ifds;
fd_set efds;
int    stat;
int    got;

#ifdef CFS_DEBUG
	fprintf(stderr, "CFSAddressImpl::readTimeout %ld bytes\n", (unsigned long)n);
#endif

	if ( n == 0 )
		return;

	do {

		FD_ZERO( &ifds ); FD_SET( fd, &ifds );
		FD_ZERO( &efds ); FD_SET( fd, &efds );

		stat = pselect( fd + 1, &ifds, 0, &efds, timeout->isIndefinite() ? 0 : &timeout->tv_, 0 );
		if ( 0 >= stat ) {
			if ( 0 == stat ) {
				throw TimeoutError("CFSAddressImpl::read timed out");
			} else {
				throw ErrnoError("CFSAddressImpl::read pselect failed");
			}
		}

		got = ::read( fd, buf, n );

		if ( got <= 0 ) {
			throw ErrnoError("CFSAddressImpl::read failed");
		}

		n   -= got;
		buf += got;

	} while ( n );

}

static void writeTimeout(int fd, uint8_t *buf, unsigned n, const CTimeout *timeout)
{
#ifdef USE_SELECT_ON_WRITE
fd_set ofds;
fd_set efds;
int    stat;
#endif
int    put;

#ifdef CFS_DEBUG
	fprintf(stderr, "CFSAddressImpl::writeTimeout %ld bytes\n", (unsigned long)n);
#endif

	if ( n == 0 )
		return;

	do {

#ifdef USE_SELECT_ON_WRITE
		FD_ZERO( &ofds ); FD_SET( fd, &ofds );
		FD_ZERO( &efds ); FD_SET( fd, &efds );

		stat = pselect( fd + 1, 0, &ofds, &efds, timeout->isIndefinite() ? 0 : &timeout->tv_, 0 );

#ifdef CFS_DEBUG
		fprintf(stderr, "CFSAddressImpl::writeTimeout select returned %i\n", stat);
#endif
		if ( 0 >= stat ) {
			if ( 0 == stat ) {
				throw TimeoutError("CFSAddressImpl::write timed out");
			} else {
				throw ErrnoError("CFSAddressImpl::write pselect failed");
			}
		}
#endif

		put = ::write( fd, buf, n );

		if ( put <= 0 ) {
			throw ErrnoError("CFSAddressImpl::write failed");
		}

		n   -= put;
		buf += put;

	} while ( n );
}
