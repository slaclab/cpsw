#include <cpsw_buf.h>
#include <boost/weak_ptr.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>

#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <stdio.h>

using boost::make_shared;

class CBufImpl;
class CBufChainImpl;
typedef shared_ptr<CBufImpl>      BufImpl;
typedef shared_ptr<CBufChainImpl> BufChainImpl;

using boost::weak_ptr;
using boost::static_pointer_cast;
using boost::lockfree::detail::freelist_stack;
using boost::atomic;
using boost::memory_order_relaxed;
using boost::memory_order_release;
using boost::memory_order_acquire;

#define NULLBUF   BufImpl( reinterpret_cast<CBufImpl*>(0) )
#define NULLCHAIN BufChainImpl( reinterpret_cast<CBufChainImpl*>(0) )

class CBufKey {
private:
	CBufKey() {};
	friend class CBufFreeList;
};

class CBufImpl : public IBuf {
private:
	weak_ptr<CBufChainImpl> chain_;
	BufImpl            next_; 
	weak_ptr<CBufImpl> prev_;
	weak_ptr<CBufImpl> self_;
	unsigned beg_,  end_;
	uint8_t  data_[3*512 - 2*sizeof(prev_) - sizeof(self_) - 2*sizeof(beg_)];
protected:
	virtual void     addToChain(BufImpl p, bool);
	virtual void     delFromChain();

public:
	CBufImpl(CBufKey k);

	virtual void     addToChain(BufChainImpl c);

	void     setSelf( BufImpl me );

	size_t   getCapacity() { return sizeof(data_);                    }
	size_t   getSize()     { return end_ - beg_;                      }
	uint8_t *getPayload()  { return data_ + beg_;                     }

	void     setSize(size_t);
	void     setPayload(uint8_t*);
	void     reinit();

	Buf      getNext()      { return next_; }
	Buf      getPrev()      { return prev_.expired() ? NULLBUF : Buf(prev_);      }

	BufImpl  getNextImpl()  { return next_; }
	BufImpl  getPrevImpl()  { return prev_.expired() ? NULLBUF : BufImpl(prev_);  }

	BufChainImpl getChain() { return chain_.expired() ? NULLCHAIN : BufChainImpl(chain_); }

	void     after(Buf);
	void     before(Buf);
	void     unlink();
	void     split();

	// We don't need to unlink a buffer when it is destroyed:
	// Only the first one in a chain can ever be destroyed (because
	// strong refs to all others in a chain exist). If this happens
	// then the 'prev' pointer of the following node expires which 
	// yields the correct result: a subsequent getPrev() on the 
	// second/following node will return NULL.
	//virtual ~CBufImpl() { }

	static BufImpl getBuf(size_t capa);
};

class CBufChainImpl : public IBufChain {
private:
	weak_ptr<CBufChainImpl> self_;
	BufImpl   head_;
	BufImpl   tail_;
	unsigned  len_;
	size_t    size_;
	virtual void setHead(BufImpl h)     { head_ = h; }
	virtual void setTail(BufImpl t)     { tail_ = t; }
	friend class CBufImpl;

protected:
	CBufChainImpl() : len_(0), size_(0) {}
	
public:

	virtual Buf      getHead()          { return head_; }
	virtual Buf      getTail()          { return tail_; }
	virtual BufImpl  getHeadImpl()      { return head_; }
	virtual BufImpl  getTailImpl()      { return tail_; }

	virtual unsigned getLen()           { return len_;  } // # of buffers in chain
	virtual size_t   getSize()          { return size_; } // in bytes

	virtual void     addSize(ssize_t s) { size_ += s; }
	virtual void     addLen(int l)      { len_  += l; }

	virtual void     setSize(size_t s)  { size_  = s; }
	virtual void     setLen(unsigned l) { len_   = l; }

	virtual Buf      createAtHead();
	virtual Buf      createAtTail();

	virtual void     addAtHead(Buf b);
	virtual void     addAtTail(Buf b);

	static BufChainImpl createImpl();
};

BufChainImpl CBufChainImpl::createImpl()
{
CBufChainImpl *c = new CBufChainImpl();
BufChainImpl  rval = BufChainImpl( c );
	rval->self_ = rval;
	return rval;
}

BufChain IBufChain::create()
{
	return CBufChainImpl::createImpl();
}


CBufImpl::CBufImpl(CBufKey k)
: beg_(0), end_(sizeof(data_))
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
unsigned     old_size = getSize();
unsigned     e;
BufChainImpl c;

	if ( (e = beg_ + s) > sizeof(data_) )
		throw InvalidArgError("requested size too big");
	end_ = e;

	if ( (c=getChain()) ) {
		c->addSize( getSize() - old_size );
	}
}

void CBufImpl::setPayload(uint8_t *p)
{
unsigned     old_size = getSize();
BufChainImpl c;

	if ( !p ) {
		beg_ = 0;
	} else if ( p < data_ || p > data_ + sizeof(data_) ) {
		throw InvalidArgError("requested payload pointer out of range");
	} else  {
		beg_ = p - data_;
		if ( end_ < beg_ )
			end_ = beg_;
	}
	if ( (c=getChain()) ) {
		c->addSize( getSize() - old_size );
	}
}

void CBufImpl::reinit()
{
unsigned     old_size = getSize();
BufChainImpl c;
	beg_ = 0;
	end_ = sizeof(data_);
	if ( (c=getChain()) ) {
		c->addSize( getSize() - old_size );
	}
}

void CBufImpl::addToChain(BufImpl p, bool before)
{
BufChainImpl c  = getChain();
BufChainImpl pc = p->getChain();

	if ( c != pc ) {
		if ( c ) {
			if ( pc )
				throw InternalError("buffers belong to different chains");
			else
				p->chain_ = c;
		} else {
			chain_ = c = pc;
		}
		if ( c ) {
			if ( before ) {
				if ( (p == c->getHeadImpl()) ) {
					BufImpl me = BufImpl( self_ );
					c->setHead( me );
				}
			} else {
				if ( (p == c->getTailImpl()) ) {
					BufImpl me = BufImpl( self_ );
					c->setTail( me );
				}
			}
			c->addLen( 1 );
			c->addSize( getSize() );
		}
	}
}

void CBufImpl::addToChain(BufChainImpl c)
{
BufImpl  p,h,t;
BufImpl  me = BufImpl(self_);
size_t   s = 0;
unsigned l = 0;
	if ( c->getHead() || c->getTail() )
		throw InternalError("can use this method only to initialize an empty chain"); 

	h = t = me;
	for ( p=me; p; t=p, p=p->getNextImpl() ) {
		if ( getChain() )
			throw InternalError("buffer already on a chain");
		s += p->getSize();
		l++;
		chain_ = c;
	}
	for ( p=me->getPrevImpl(); p; h=p, p=p->getPrevImpl() ) {
		if ( getChain() )
			throw InternalError("buffer already on a chain");
		s += p->getSize();
		l++;
		chain_ = c;
	}
	c->setSize( s );
	c->setLen ( l );
	c->setHead( h );
	c->setTail( t );
}

void CBufImpl::delFromChain()
{
BufChainImpl c  = getChain();
	if ( c ) {
		BufImpl me = BufImpl( self_ );
		if ( c->getHeadImpl() == me ) {
			c->setHead( c->getHeadImpl()->getNextImpl() );
		}
		if ( c->getTailImpl() == me ) {
			c->setTail( c->getTailImpl()->getPrevImpl() );
		}
		c->addLen( -1 );
		c->addSize( - getSize() );
		chain_.reset();
	}
}

void CBufImpl::after(Buf p)
{
BufImpl pi = static_pointer_cast<BufImpl::element_type>(p);
BufImpl me = BufImpl(self_);

	if ( pi && pi != me ) {

		if ( getNextImpl() || getPrevImpl() )
			throw InternalError("Cannot enqueue non-empty node");

		addToChain( pi, false ); // no exceptions must happen after this

		prev_        = pi;
		next_        = pi->next_;
		pi->next_    = me;
		if ( getNextImpl() )
			getNextImpl()->prev_ = me;

	}
}

void CBufImpl::before(Buf p)
{
BufImpl pi = static_pointer_cast<BufImpl::element_type>(p);
BufImpl me = BufImpl(self_);

	if ( pi && pi != me ) {
		if ( getNextImpl() || getPrevImpl() )
			throw InternalError("Cannot enqueue non-empty node");

		addToChain( pi, true); // no exceptions must happen after this

		next_        = pi;
		prev_        = pi->prev_;
		pi->prev_    = me;
		if ( getPrevImpl() )
			getPrevImpl()->next_ = me;
	}
}

void CBufImpl::unlink()
{
	if ( getNextImpl() ) {
		getNextImpl()->prev_ = prev_;
		next_ = NULLBUF;
	}
	if ( getPrevImpl() ) {
		getPrevImpl()->next_ = next_;
		prev_ = NULLBUF;
	}
	delFromChain(); // no exceptions must happen after this
}

void CBufImpl::split()
{
	if ( getChain() ) {
		throw InternalError("Cannot split buffers which are on a chain");
	}
	if ( getPrevImpl() ) {
		getPrevImpl()->next_ = NULLBUF;
		prev_                = NULLBUF;
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
	
void CBufChainImpl::addAtHead(Buf b)
{
	if ( ! head_ ) {
		static_pointer_cast<BufImpl::element_type>(b)->addToChain( BufChainImpl(self_) );
	} else {
		b->before( head_ );
	}
}

void CBufChainImpl::addAtTail(Buf b)
{
	if ( ! tail_ ) {
		static_pointer_cast<BufImpl::element_type>(b)->addToChain( BufChainImpl(self_) );
	} else {
		b->after( tail_ );
	}
}

Buf CBufChainImpl::createAtHead()
{
	Buf rval = IBuf::getBuf();
	addAtHead(rval);
	return rval;
}

Buf CBufChainImpl::createAtTail()
{
	Buf rval = IBuf::getBuf();
	addAtTail(rval);
	return rval;
}
