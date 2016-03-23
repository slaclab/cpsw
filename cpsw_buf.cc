#include <boost/weak_ptr.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>

#include <cpsw_api_user.h>
#include <cpsw_error.h>
#include <cpsw_freelist.h>
#include <cpsw_buf.h>

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

class CBufImpl : public IBuf, public CFreeListNode<CBufImpl> {
private:
	weak_ptr<CBufChainImpl> chain_;
	BufImpl                 next_; 
	weak_ptr<CBufImpl>      prev_;
	unsigned short          beg_,  end_, capa_;
	// no alignment of the data area is guaranteed - but this is not necessary
	// as we always treat it what it is: a raw array of bytes.
	// Any conversion to or from more structured types (including cardinals)
	// should/must be performed explicitly!
	uint8_t  data_[];
protected:
	virtual void     addToChain(BufImpl p, bool);
	virtual void     delFromChain();

public:
	CBufImpl(CFreeListNodeKey<CBufImpl> k, unsigned short capa);

	virtual void     addToChain(BufChainImpl c);

	virtual size_t   getCapacity() { return capa_; }

	virtual size_t   getSize()     { return end_ - beg_;                      }
	virtual size_t   getAvail()    { return getCapacity() - end_;             }
	virtual size_t   getHeadroom() { return beg_;                             }
	virtual uint8_t *getPayload()  { return data_ + beg_;                     }

	virtual void     setSize(size_t);
	virtual void     setPayload(uint8_t*);
	virtual void     reinit();
	virtual bool     adjPayload(ssize_t);

	virtual Buf      getNext()      { return next_; }
	virtual Buf      getPrev()      { return prev_.expired() ? NULLBUF : Buf(prev_);      }

	virtual BufImpl  getNextImpl()  { return next_; }
	virtual BufImpl  getPrevImpl()  { return prev_.expired() ? NULLBUF : BufImpl(prev_);  }

	virtual BufChainImpl getChainImpl() { return chain_.expired() ? NULLCHAIN : BufChainImpl(chain_); }
	virtual BufChain getChain();

	virtual void     after(Buf);
	virtual void     before(Buf);
	virtual void     unlink();
	virtual void     split();

	// We don't need to unlink a buffer when it is destroyed:
	// Only the first one in a chain can ever be destroyed (because
	// strong refs to all others in a chain exist). If this happens
	// then the 'prev' pointer of the following node expires which 
	// yields the correct result: a subsequent getPrev() on the 
	// second/following node will return NULL.
	//virtual ~CBufImpl() { }

	static const size_t HEADROOM = 32; // enough for rssi + packetizer

	static BufImpl getBuf(size_t capa, bool clip = false);
};

static CFreeList<CBufImpl> freeListBig  (0, 2048 - sizeof(CBufImpl));

// free lists ordered in decreasing order of node size
static CFreeList<CBufImpl> *freeListPool[] = {
	&freeListBig
};

class CBufChainImpl : public IBufChain, public CFreeListNode<CBufChainImpl> {
private:
	BufChainImpl strong_self_;
	BufImpl      head_;
	BufImpl      tail_;
	unsigned     len_;
	size_t       size_;

	virtual void setHead(BufImpl h)     { head_ = h; }
	virtual void setTail(BufImpl t)     { tail_ = t; }

	BufChain     yield_ownership();

	friend class CBufImpl;
	friend void  IBufChain::take_ownership(BufChain);

public:

	CBufChainImpl( CFreeListNodeKey<CBufChainImpl> k );

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

	virtual Buf      createAtHead(size_t capa, bool clip = false);
	virtual Buf      createAtTail(size_t capa, bool clip = false);

	virtual void     addAtHead(Buf b);
	virtual void     addAtTail(Buf b);

	virtual uint64_t extract(void *buf, uint64_t off, uint64_t size);
	virtual void     insert(void *buf, uint64_t off, uint64_t size);

	static BufChainImpl createImpl();

	static CFreeList<CBufChainImpl> freeList;
};

CFreeList<CBufChainImpl> CBufChainImpl::freeList;

CBufImpl::CBufImpl(CFreeListNodeKey<CBufImpl> k, unsigned short capa)
: CFreeListNode<CBufImpl>( k ),
  beg_(HEADROOM),
  end_(HEADROOM),
  capa_(capa)
{
}

void CBufImpl::setSize(size_t s)
{
unsigned     old_size = getSize();
unsigned     e;
BufChainImpl c;

	if ( (e = beg_ + s) > getCapacity() )
		throw InvalidArgError("requested size too big");
	end_ = e;

	if ( (c=getChainImpl()) ) {
		c->addSize( getSize() - old_size );
	}
}

void CBufImpl::setPayload(uint8_t *p)
{
unsigned     old_size = getSize();
BufChainImpl c;

	if ( !p ) {
		beg_ = 0;
	} else if ( p < data_ || p > data_ + getCapacity() ) {
		throw InvalidArgError("requested payload pointer out of range");
	} else  {
		beg_ = p - data_;
		if ( end_ < beg_ )
			end_ = beg_;
	}
	if ( (c=getChainImpl()) ) {
		c->addSize( getSize() - old_size );
	}
}

bool CBufImpl::adjPayload(ssize_t delta)
{
unsigned old_size = getSize();
BufChainImpl c;

	if ( delta > beg_ || delta > old_size )
		return false;

	beg_ += delta;

	if ( (c=getChainImpl()) ) {
		c->addSize( getSize() - old_size );
	}
	return true;
}

void CBufImpl::reinit()
{
unsigned     old_size = getSize();
BufChainImpl c;
	beg_ = end_ = HEADROOM;
	if ( (c=getChainImpl()) ) {
		c->addSize( getSize() - old_size );
	}
}

void CBufImpl::addToChain(BufImpl p, bool before)
{
BufChainImpl c  = getChainImpl();
BufChainImpl pc = p->getChainImpl();

	if ( c )
		throw InternalError("buffer already on a chain");

	if ( pc ) {
		chain_ = c = pc;
		if ( before ) {
			if ( (p == c->getHeadImpl()) ) {
				BufImpl me = getSelf();
				c->setHead( me );
			}
		} else {
			if ( (p == c->getTailImpl()) ) {
				BufImpl me = getSelf();
				c->setTail( me );
			}
		}
		c->addLen( 1 );
		c->addSize( getSize() );
	}
}

void CBufImpl::addToChain(BufChainImpl c)
{
BufImpl  p,h,t;
BufImpl  me = getSelf();
size_t   s = 0;
unsigned l = 0;
	if ( c->getHead() || c->getTail() )
		throw InternalError("can use this method only to initialize an empty chain"); 

	h = t = me;
	for ( p=me; p; t=p, p=p->getNextImpl() ) {
		if ( p->getChainImpl() )
			throw InternalError("buffer already on a chain");
		s += p->getSize();
		l++;
		p->chain_ = c;
	}
	for ( p=me->getPrevImpl(); p; h=p, p=p->getPrevImpl() ) {
		if ( p->getChainImpl() )
			throw InternalError("buffer already on a chain");
		s += p->getSize();
		l++;
		p->chain_ = c;
	}
	c->setSize( s );
	c->setLen ( l );
	c->setHead( h );
	c->setTail( t );
}

BufChain CBufImpl::getChain()
{
	return getChainImpl();
}

void CBufImpl::delFromChain()
{
BufChainImpl c  = getChainImpl();
	if ( c ) {
		BufImpl me = getSelf();
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
BufImpl me = getSelf();

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
BufImpl me = getSelf();

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
	delFromChain(); // no exceptions must happen after this

	if ( getNextImpl() ) {
		getNextImpl()->prev_ = prev_;
		next_ = NULLBUF;
	}
	if ( getPrevImpl() ) {
		getPrevImpl()->next_ = next_;
		prev_ = NULLBUF;
	}
}

void CBufImpl::split()
{
	if ( getChainImpl() ) {
		throw InternalError("Cannot split buffers which are on a chain");
	}
	if ( getPrevImpl() ) {
		getPrevImpl()->next_ = NULLBUF;
		prev_                = NULLBUF;
	}
}

BufImpl CBufImpl::getBuf(size_t capa, bool clip)
{
unsigned maxcap = freeListPool[0]->getExtraSize();

CFreeList<CBufImpl> *flp = &freeListBig;

	if ( CAPA_MAX == capa )
		capa = maxcap;

	if ( capa > maxcap ) {
		if ( clip ) {
			capa = maxcap;
		} else {
			throw InternalError("ATM all buffers are std. MTU size");
		}
	}

	return flp->alloc( flp->getExtraSize() );
}

Buf IBuf::getBuf(size_t capa, bool clip)
{
	return CBufImpl::getBuf( capa, clip );
}

unsigned IBuf::numBufsAlloced()
{
unsigned rval, i;
	for ( i=0, rval=0; i<sizeof(freeListPool)/sizeof(freeListPool[0]); i++ ) {
		rval += freeListPool[i]->getNumAlloced();
	}
	return rval;
}

unsigned IBuf::numBufsFree()
{
unsigned rval, i;
	for ( i=0, rval=0; i<sizeof(freeListPool)/sizeof(freeListPool[0]); i++ ) {
		rval += freeListPool[i]->getNumFree();
	}
	return rval;
}

unsigned IBuf::numBufsInUse()
{
unsigned rval, i;
	for ( i=0, rval=0; i<sizeof(freeListPool)/sizeof(freeListPool[0]); i++ ) {
		rval += freeListPool[i]->getNumInUse();
	}
	return rval;
}
CBufChainImpl::CBufChainImpl( CFreeListNodeKey<CBufChainImpl> k )
: CFreeListNode<CBufChainImpl>( k ),
  len_(0),
  size_(0)
{
}

	
BufChainImpl CBufChainImpl::createImpl()
{
	return freeList.alloc();
}

BufChain IBufChain::create()
{
	return CBufChainImpl::createImpl();
}

void CBufChainImpl::addAtHead(Buf b)
{
	if ( ! head_ ) {
		static_pointer_cast<BufImpl::element_type>(b)->addToChain( getSelf() );
	} else {
		b->before( head_ );
	}
}

void CBufChainImpl::addAtTail(Buf b)
{
	if ( ! tail_ ) {
		static_pointer_cast<BufImpl::element_type>(b)->addToChain( getSelf() );
	} else {
		b->after( tail_ );
	}
}

BufChain CBufChainImpl::yield_ownership()
{
BufChain rval = strong_self_;
	strong_self_.reset();
	return rval;
}

void IBufChain::take_ownership(BufChain current_owner)
{
BufChainImpl me = static_pointer_cast<BufChainImpl::element_type>( current_owner );
	me->strong_self_ = me;
}

Buf CBufChainImpl::createAtHead(size_t size, bool clip)
{
	Buf rval = IBuf::getBuf(size, clip);
	addAtHead(rval);
	return rval;
}

Buf CBufChainImpl::createAtTail(size_t size, bool clip)
{
	Buf rval = IBuf::getBuf(size, clip);
	addAtTail(rval);
	return rval;
}


uint64_t CBufChainImpl::extract(void *buf, uint64_t off, uint64_t size)
{
uint64_t rval = 0;
uint8_t *dst  = static_cast<uint8_t*>(buf);
Buf      b;

	if ( ! (b = getHead()) )
		return 0;

	while ( b->getSize() <= off ) {
		off -= b->getSize();	
		if ( ! (b = b->getNext()) )
			return 0;
	}

	do {

		unsigned l = b->getSize() - off;

		if ( l > size )
			l = size;

		memcpy( dst, b->getPayload() + off, l );
		rval   += l;
		dst    += l;
		size   -= l;

		if ( ! (b = b->getNext()) ) {
			return rval;
		}

		off = 0;

	} while ( size > 0 );

	return rval;
}

void CBufChainImpl::insert(void *buf, uint64_t off, uint64_t size)
{
uint64_t delta;
uint8_t *src  = static_cast<uint8_t*>(buf);
Buf      b, nxtb;

	// skip over existing data
	b = getHead();

	nxtb = Buf( static_cast<Buf::element_type*>(NULL) );

	while ( b ) {
		delta = b->getSize();
		if ( off <= delta ) {
			delta = off;
			// truncate
			b->setSize( delta );
			off   = 0;
			// leave loop with b pointing to this buffer
			// (since it has space available)
			nxtb = b->getNext();
			break;
		}
		off -= delta;
		b    = b->getNext();
	}

	// add dummy buffers
	while ( off > 0 ) {
		// off > 0 implies !b (and !nextb)
		uint64_t delta;
		b = createAtTail( off, true );
		delta = b->getCapacity();
		if ( off < delta )
			delta = off;
		b->setSize(delta);
		off -= delta;
	}

	// at this point: off == 0
	//
	// if b != null then it points to the last 
	// buffer with valid old data in it.
	// if nxtb != null then it points to the first
	// buffer that needs to be truncated

	while ( size > 0 ) {

		if ( ! b ) {
			b = createAtTail( size, true );
		}

		// there might be a on old buffer to overwrite...

		if ( (delta = b->getAvail()) > 0 ) {

			if ( delta > size )
				delta = size;

			memcpy( b->getPayload() + b->getSize(), src, delta );

			b->setSize( b->getSize() + delta );

			src  += delta;
			size -= delta;
		}

		b = nxtb = b->getNext();
		if ( b )
			b->reinit();
	}

	// at this point: if nxtb is not NULL that means we
	// are truncating an existing chain.
	while ( nxtb ) {
		b    = nxtb;
		nxtb = b->getNext();	
		b->unlink();
	}
}
