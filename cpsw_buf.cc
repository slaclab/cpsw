#include <cpsw_buf.h>
#include <boost/weak_ptr.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/atomic.hpp>

#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <stdio.h>

class CBufImpl;
typedef shared_ptr<CBufImpl> BufImpl;

using boost::weak_ptr;
using boost::static_pointer_cast;
using boost::lockfree::detail::freelist_stack;
using boost::atomic;
using boost::memory_order_relaxed;
using boost::memory_order_release;
using boost::memory_order_acquire;

#define NULLBUF BufImpl( reinterpret_cast<CBufImpl*>(0) )

class CBufKey {
private:
	CBufKey() {};
	friend class CBufFreeList;
};

class CBufImpl : public IBuf {
private:
	BufImpl prev_, next_; 
	weak_ptr<CBufImpl> self_;
	unsigned beg_,  end_;
	uint8_t  data_[3*512 - 2*sizeof(prev_) - sizeof(self_) - 2*sizeof(beg_)];

public:
	CBufImpl(CBufKey k);

	void     setSelf( BufImpl me );

	size_t   getCapacity() { return sizeof(data_);                    }
	size_t   getSize()     { return end_ - beg_;                      }
	uint8_t *getPayload()  { return data_ + beg_;                     }

	void     setSize(size_t);
	void     setPayload(uint8_t*);
	void     reinit();

	Buf      getNext()     { return next_; }
	Buf      getPrev()     { return prev_; }

	void     after(Buf);
	void     before(Buf);
	void     unlink();
	void     split();

	static BufImpl getBuf(size_t capa);
};

CBufImpl::CBufImpl(CBufKey k)
:beg_(0), end_(sizeof(data_))
{
}

void CBufImpl::setSelf( BufImpl me )
{
	if ( ! self_.expired() || me.get() != this ) 
		throw InternalError("Self pointer already set or not matching 'this'");
	self_ = me;
}

void CBufImpl::setSize(size_t s)
{
unsigned e;
	if ( (e = beg_ + s) > sizeof(data_) )
		throw InvalidArgError("requested size too big");
	end_ = e;
}

void CBufImpl::setPayload(uint8_t *p)
{
	if ( !p ) {
		beg_ = 0;
	} else if ( p < data_ || p > data_ + sizeof(data_) ) {
		throw InvalidArgError("requested payload pointer out of range");
	} else  {
		beg_ = p - data_;
		if ( end_ < beg_ )
			end_ = beg_;
	}
}

void CBufImpl::reinit()
{
	beg_ = 0;
	end_ = sizeof(data_);
}

void CBufImpl::after(Buf p)
{
BufImpl pi = static_pointer_cast<BufImpl::element_type>(p);
BufImpl me = BufImpl(self_);

	if ( pi && pi != me ) {
		if ( next_ || prev_ )
			throw InternalError("Cannot enqueue non-empty node");
		prev_        = pi;
		next_        = pi->next_;
		pi->next_    = me;
		if ( next_ )
			next_->prev_ = me;
	}
}

void CBufImpl::before(Buf p)
{
BufImpl pi = static_pointer_cast<BufImpl::element_type>(p);
BufImpl me = BufImpl(self_);

	if ( pi && pi != me ) {
		if ( next_ || prev_ )
			throw InternalError("Cannot enqueue non-empty node");
		next_        = pi;
		prev_        = pi->prev_;
		pi->prev_    = me;
		if ( prev_ )
			prev_->next_ = me;
	}
}

void CBufImpl::unlink()
{
	if ( next_ ) {
		next_->prev_ = prev_;
		next_ = NULLBUF;
	}
	if ( prev_ ) {
		prev_->next_ = next_;
		prev_ = NULLBUF;
	}
}

void CBufImpl::split()
{
	if ( prev_ ) {
		prev_->next_ = NULLBUF;
		prev_        = NULLBUF;
	}
}

class CBufAlloc : public std::allocator<CBufImpl> {
private:
	atomic<unsigned int> *nAlloc_p_;
	atomic<unsigned int> *nFree_p_;
public:
	CBufAlloc( atomic<unsigned int> *nAlloc_p, atomic<unsigned int> *nFree_p )
	:nAlloc_p_(nAlloc_p), nFree_p_(nFree_p)
	{
	}

	CBufImpl *allocate(size_t n)
	{
		nAlloc_p_->fetch_add(n, memory_order_release);
#ifndef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
		nFree_p_->fetch_add(n, memory_order_release);
#endif
		return std::allocator<CBufImpl>::allocate( n );
	}
};

// g++ (4.8.4) seems to get confused (as I do) by all the template stuff.
// These typedefs seem to work around it; have no idea why (found by trial + error).
typedef CBufAlloc                           ALL;
typedef freelist_stack<CBufImpl, CBufAlloc> FreeListBase;

class CBufFreeList;

class DTOR {
private:
	CBufFreeList *fl_;
public:
	DTOR(CBufFreeList *fl) : fl_ (fl) {}
	void operator()(CBufImpl *o);
};


class CBufFreeList : public FreeListBase
{
private:
	atomic<unsigned int> nAlloc_;
	atomic<unsigned int> nFree_;
public:
	CBufFreeList(int n = 0)
	:FreeListBase( ALL( (nAlloc_=0, &nAlloc_ ), (nFree_ = 0, &nFree_) ) , n)
	{}

	unsigned int getNumAlloced()
	{
		return nAlloc_.load( memory_order_acquire );
	}

	unsigned int getNumFree()
	{
		return nFree_.load( memory_order_acquire );
	}

	unsigned int getNumInUse()
	{
		// can't atomically read both -- since this is a diagnostic
		// we don't care about marginal race conditions here.
		unsigned int rval = getNumAlloced();
		rval -= getNumFree();
		return rval;
	}

	CBufImpl *construct(CBufKey k)
	{
		const bool ThreadSafe = true;
		const bool Bounded    = false;
		CBufImpl *rval = FreeListBase::construct<ThreadSafe,Bounded>( k );
		if ( rval )
			nFree_.fetch_sub( 1, memory_order_release );
		return rval;
	}

	void destruct(CBufImpl *o)
	{
		const bool ThreadSafe = true;
		nFree_.fetch_add( 1, memory_order_release );
		FreeListBase::destruct<ThreadSafe>( o );
	}

	BufImpl getBuf()
	{
		CBufImpl *b = construct( CBufKey() );
		if ( ! b )
			throw InternalError("Unable to allocate Buffer");
		BufImpl rval = BufImpl( b, DTOR(this) );
		rval->setSelf( rval );
		return rval;
	}
};

void DTOR::operator()(CBufImpl *o)
{
	fl_->destruct( o );
}

static CBufFreeList fl;

BufImpl CBufImpl::getBuf(size_t capa)
{
	if ( capa > sizeof(data_) ) {
		throw InternalError("ATM all buffers are std. MTU size");
	}
	return fl.getBuf();
}

Buf IBuf::getBuf(size_t capa)
{
	return CBufImpl::getBuf( capa );
}

unsigned IBuf::numBufsAlloced()
{
	return fl.getNumAlloced();
}

unsigned IBuf::numBufsFree()
{
	return fl.getNumFree();
}

unsigned IBuf::numBufsInUse()
{
	return fl.getNumInUse();
}
