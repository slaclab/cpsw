 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_FREE_LIST_H
#define CPSW_FREE_LIST_H

#include <boost/weak_ptr.hpp>
#include <boost/lockfree/stack.hpp>
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>

#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <stdint.h>

// Extend boost's lockless free-list to manage objects that are
// handed out via smart/shared pointers.
// Also: keep statistics

using boost::weak_ptr;
using boost::static_pointer_cast;
using boost::lockfree::detail::freelist_stack;
using boost::atomic;
using boost::memory_order_relaxed;
using boost::memory_order_release;
using boost::memory_order_acquire;
using boost::make_shared;
using boost::allocate_shared;

template <typename, typename> class CFreeList;

template <typename T> class CFreeListNodeKey {
private:
	CFreeListNodeKey() {};
	template <typename, typename> friend class CFreeList;
};

template <typename C> class CFreeListNode {
private:
	weak_ptr<C> self_;

public:
	virtual void setSelf(shared_ptr<C> me)
	{
		if ( ! self_.expired() || me.get() != this )
			throw InternalError("Self pointer already set or not matching 'this'");
		self_ = me;
	}

	template <typename, typename> friend class CFreeList;

public:

	CFreeListNode( const CFreeListNodeKey<C> &k )
	{
	}

	virtual shared_ptr<C> getSelf()
	{
		return shared_ptr<C>( self_ );
	}

	template <typename AS> AS getSelfAs()
	{
		return static_pointer_cast<typename AS::element_type>( getSelf() );
	}

	virtual ~CFreeListNode() {}
};

class CFreeListStats {
protected:
	atomic<unsigned int> nAlloc_;
	atomic<unsigned int> nFree_;

private:
	CFreeListStats(const CFreeListStats &);
	CFreeListStats& operator=(const CFreeListStats &);

public:
	CFreeListStats()
	: nAlloc_(0), nFree_(0)
	{
	}

	void addNumAlloced(unsigned n = 1)
	{
		nAlloc_.fetch_add(n, memory_order_release);
	}

	void addNumFree(unsigned n = 1)
	{
		nFree_.fetch_add(n, memory_order_release);
	}

	void subNumAlloced(unsigned n = 1)
	{
		nAlloc_.fetch_sub(n, memory_order_release);
	}

	void subNumFree(unsigned n = 1)
	{
		nFree_.fetch_sub(n, memory_order_release);
	}
		
	unsigned int getNumAlloced()
	{
		return nAlloc_.load( memory_order_acquire );
	}

	unsigned int getNumFree()
	{
		return nFree_.load( memory_order_acquire );
	}

};

typedef CFreeListStats *FreeListStats;

// Allocate objects from the heap (when free-list needs to be populated)
template <typename C> class CFreeListRawNodeAlloc {
private:
	FreeListStats stats_;
public:
	CFreeListRawNodeAlloc( FreeListStats stats )
	: stats_ (stats )
	{
	}

	C *allocate(size_t n)
	{
		if ( n != 1 ) {
			throw std::bad_alloc();
		}
		stats_->addNumAlloced(n);
#ifndef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
		stats_->addNumFree(n);
#endif
		return (C*) ::malloc( sizeof(C)*n );
	}

	void deallocate(C *o, size_t n)
	{
		if ( n != 1 ) {
			throw std::bad_alloc();
		}
		::free( o );
	}

};

class IFreeListPool : public CFreeListStats {
public:
	virtual void *allocate()         = 0;
	virtual void deallocate(void *)  = 0;
	virtual unsigned size()          = 0;
};

typedef IFreeListPool *FreeListPool;

template <std::size_t SZ>
class CFreeListRaw :
  public IFreeListPool,
  public freelist_stack< uint8_t[SZ], CFreeListRawNodeAlloc< uint8_t[SZ] > >
{
public:
	typedef uint8_t                        T[SZ];
	typedef CFreeListRawNodeAlloc<T>       ALL;
	typedef freelist_stack<T, ALL>         BASE;
	typedef shared_ptr< CFreeListRaw<SZ> > SP;

	CFreeListRaw()
	: BASE( ALL( this ) , 0 )
	{
		fprintf(stderr, "CFreeListRaw CONS\n");
	}

	~CFreeListRaw()
	{
		fprintf(stderr, "CFreeListRaw DEST\n");
	}

	virtual void *allocate()
	{
	const bool ThreadSafe = true;
	const bool Bounded    = false;
		T *rval = BASE::template allocate<ThreadSafe, Bounded>();
		if ( rval )
			subNumFree();
		return rval;
	}

	virtual unsigned size()
	{
		return SZ;
	}

	virtual void deallocate(void *p)
	{
	const bool ThreadSafe = true;
		addNumFree();
		BASE::template deallocate<ThreadSafe>( static_cast< T* >( p ) );
	}

	static FreeListPool pool()
	{
		// lives forever
		static FreeListPool thepool = new typename SP::element_type () ; 
		return thepool;
	}
};

// CFreeListNodeAlloc is a c++ allocator which uses a free-list
// to obtain the memory.
// Here are some subtleties:
//
//  1 We want to use a single allocation for shared_ptr's control block
//    and the user data it points to. Both should come from the free list.
//  2 The only way to achieve 1) is by using allocate_shared().
//  3 Allocate_shared() must be passed a c++ 'allocator'
//  4 The allocator is 'rebound' to an unknown/opaque type. I.e.,
//    if we pass  allocate_shared< TYPE > ( Allocator<TYPE>(), args ) then
//    this does ***not*** result in a call to Allocator<TYPE>::allocate()
//    but to  Allocator<OPAQUE>::allocate().
//  5 We want to maintain some statistics for the free-list.
//  6 because of 4) there is no easy way to get to the 'true' freelist, i.e.,
//    the one used by Allocator<OPAQUE>::allocate() since there is no way
//    to find out what 'OPAQUE' actually is (knowing its size could be enough
//    to locate the associated free-list).
//  7 The ONLY code that gets to 'see' OPAQUE is actually Allocator<OPAQUE>::allocate().
//  8 AGAIN: Something like Allocator<TYPE>::getFreeList() would NOT return
//    the correct free-list but one to allocate objects of TYPE (not OPAQUE)!
//  9 Since any shared_ptr must have a reference to the correct allocator
//    (in order to destroy) it should be possible to get a handle to it but
//    shared_ptr unfortunately does not expose this :-(
//
// Thus, we resort to an ugly hack:
// Our 'Allocator' holds the address of a pointer to the free-list (initialized
// to NULL, because free-list initially unknown) this address is passed by
// the copy constructor (which rebinds to a different allocator) and the
// pointer to the free-list is passed out by the correct
// Allocator<OPAQUE>::allocate():
//
//   FreeList ptr;
//
//   template <typename T> class Allocator {
//   public:
//      FreeList *pp;
//      Allocator(FreeList *pp) : pp( pp ) {}
//      template <typename U> Allocator(const Allocator<U> &o) : pp(o.pp) {}
//      U *allocate(...)
//      {
//        return (*pp = freeList)->allocate(..);
//      }
//  }
//
// Quite ugly, but the only way I could find to make this work.
//
// NOTE: The implementation below is not thread-safe (with regard of the pointer
// hack in question, that is), and doesn't use shared_ptr for FreeList.
// This is deemed acceptable because the counters are for diagnostic purposes only.
//
// NODE must be derived from CFreeListNode !
template <typename NODE, unsigned EXTRA = 0>
class CFreeListNodeAlloc
{
private:
	FreeListPool  *hack_;

	CFreeListNodeAlloc<NODE, EXTRA> &operator=(const CFreeListNodeAlloc<NODE, EXTRA> &);

public:

	template <typename U>
	struct rebind {
		typedef CFreeListNodeAlloc<U, EXTRA> other;
	};

	CFreeListNodeAlloc(FreeListPool *hack)
	: hack_( hack )
	{
	}

	virtual unsigned getExtraSize()
	{
		return EXTRA;
	}

	FreeListPool *getPoolPtr() const
	{
		return hack_;
	}

	template <typename U>
	CFreeListNodeAlloc( const CFreeListNodeAlloc<U, EXTRA> &orig)
	: hack_( orig.getPoolPtr() )
	{
	}

	// Templates to find the right size list

	virtual NODE *
	allocate( std::size_t nelms, const void *hint)
	{
		if ( nelms != 1 ) {
			throw std::bad_alloc();
		}
		FreeListPool pool = CFreeListRaw<sizeof(NODE) + EXTRA>::pool();
		*hack_ = pool;
		return static_cast<NODE*>( pool->allocate() );
	}

	virtual void
	deallocate( NODE *p, std::size_t nelms)
	{
		if ( nelms != 1 ) {
			throw std::bad_alloc();
		}
		FreeListPool pool = CFreeListRaw<sizeof(NODE) + EXTRA>::pool();
		pool->deallocate( p );
	}

};

template <typename NODE, typename NODEBASE = NODE>
class CFreeList {
private:
	IFreeListPool *pool_;

	CFreeList(const CFreeList &);
	CFreeList & operator=(const CFreeList &);

public:

	CFreeList()
	: pool_(0)
	{
	}

	virtual unsigned int getNumAlloced()
	{
		return pool_ ? pool_->getNumAlloced() : 0;
	}

	virtual unsigned int getNumFree()
	{
		return pool_ ? pool_->getNumFree() : 0;
	}

	virtual unsigned int size()
	{
		return pool_ ? pool_->size() : 0;
	}

    virtual unsigned int getNumInUse()
    {
        // can't atomically read both -- since this is a diagnostic
        // we don't care about marginal race conditions here.
        unsigned int rval = getNumAlloced();
        rval -= getNumFree();
        return rval;
    }


	template <typename T, unsigned E, typename B> shared_ptr<T>
	allocshared()
	{
		return allocate_shared<T>( CFreeListNodeAlloc<T,E>( &pool_ ), CFreeListNodeKey<B>() );
	}

	template <typename T, unsigned E, typename B> shared_ptr<T>
	allocsharedextra()
	{
		return allocate_shared<T>( CFreeListNodeAlloc<T,E>( &pool_ ), CFreeListNodeKey<B>(), E );
	}

	shared_ptr<NODE>
	alloc()
	{
		shared_ptr<NODE> rval = allocshared<NODE, 0, NODEBASE>();
		if ( ! rval )
			throw InternalError("Unable to allocate Buffer");
		rval->setSelf( rval );
		return rval;
	}

	// Note: freelist.hpp doesn't support more than 2 arguments
	template <typename ARG> shared_ptr<NODE> alloc(ARG a)
	{
		shared_ptr<NODE> rval = allocate_shared<NODE>( CFreeListNodeAlloc<NODE>( &pool_ ), CFreeListNodeKey<NODEBASE>(), a );
		if ( ! rval )
			throw InternalError("Unable to allocate Buffer");
		rval->setSelf( rval );
		return rval;
	}
};

template <typename NODE, typename NODEBASE=NODE>
class IFreeListExtra : public CFreeList<NODE, NODEBASE> {
public:
	virtual unsigned          getExtraSize() = 0;
	virtual shared_ptr<NODE>  get()          = 0;
};

template <typename NODE, unsigned EXTRA = 0, typename NODEBASE = NODE>
class CFreeListExtra : public IFreeListExtra<NODE, NODEBASE> {
public:

	virtual unsigned int getExtraSize()
	{
		return EXTRA;
	}

	virtual shared_ptr<NODE> get()
	{
		return CFreeList<NODE, NODEBASE>::template allocsharedextra<NODE, EXTRA, NODEBASE>();
	}
};

#endif
