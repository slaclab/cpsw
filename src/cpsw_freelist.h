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

template <typename T> class CFreeList;

template <typename T> class CFreeListNodeKey {
private:
	CFreeListNodeKey() {};
	friend class CFreeList<T>;
};

template <typename C> class CFreeListNode {
private:
	weak_ptr<C> self_;

protected:
	virtual void setSelf(shared_ptr<C> me)
	{
		if ( ! self_.expired() || me.get() != this )
			throw InternalError("Self pointer already set or not matching 'this'");
		self_ = me;
	}

	friend shared_ptr<C> CFreeList<C>::alloc();
	template <typename ARG> friend shared_ptr<C> CFreeList<C>::alloc(ARG);

public:

	CFreeListNode( CFreeListNodeKey<C> k )
	{
	}

	virtual shared_ptr<C> getSelf()
	{
		return shared_ptr<C>( self_ );
	}


	virtual ~CFreeListNode() {}
};

template <typename C> class CFreeListNodeAlloc {
private:
	atomic<unsigned int> *nAlloc_p_;
	atomic<unsigned int> *nFree_p_;
	unsigned              extra_;
public:
	CFreeListNodeAlloc( atomic<unsigned int> *nAlloc_p, atomic<unsigned int> *nFree_p, unsigned extra )
	:nAlloc_p_(nAlloc_p), nFree_p_(nFree_p), extra_(extra)
	{
	}

	C *allocate(size_t n)
	{
		nAlloc_p_->fetch_add(n, memory_order_release);
#ifndef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
		nFree_p_->fetch_add(n, memory_order_release);
#endif
		return (C*)::malloc( sizeof(C)*n + extra_ );
	}

	class BadDealloc {};

	void deallocate(C *o, size_t n)
	{
		if ( n != 1 ) {
			throw BadDealloc();
		}
		::free( o );
	}

};

// NODE must be derived from CFreeListNode !
template <typename NODE>
class CFreeList : public freelist_stack< NODE, CFreeListNodeAlloc<NODE> >
{
private:
	atomic<unsigned int> nAlloc_;
	atomic<unsigned int> nFree_;
	unsigned             extra_;

protected:
	class DTOR {
		private:
			CFreeList *fl_;

			DTOR(CFreeList *fl)
			: fl_ (fl)
			{
			}

			friend class CFreeList;

		public:
			void operator()(NODE *o)
			{
				fl_->destruct( o );
			}
	};


public:
	typedef CFreeListNodeAlloc<NODE> ALLOC;
	typedef freelist_stack< NODE, ALLOC > BASE;
	CFreeList(int n = 0, unsigned extra = 0)
	:BASE( ALLOC( (nAlloc_=0, &nAlloc_ ), (nFree_ = 0, &nFree_), extra ) , n),
	 extra_( extra )
	{ }

	unsigned int getNumAlloced()
	{
		return nAlloc_.load( memory_order_acquire );
	}

	unsigned int getNumFree()
	{
		return nFree_.load( memory_order_acquire );
	}

	unsigned getExtraSize()
	{
		return extra_;
	}

	unsigned int getNumInUse()
	{
		// can't atomically read both -- since this is a diagnostic
		// we don't care about marginal race conditions here.
		unsigned int rval = getNumAlloced();
		rval -= getNumFree();
		return rval;
	}

	NODE *construct(CFreeListNodeKey<NODE> k)
	{
		const bool ThreadSafe = true;
		const bool Bounded    = false;
		NODE *rval = BASE::template construct<ThreadSafe,Bounded>( k );
		if ( rval )
			nFree_.fetch_sub( 1, memory_order_release );
		return rval;
	}

	template <typename ARG> NODE *construct(CFreeListNodeKey<NODE> k, ARG arg)
	{
		const bool ThreadSafe = true;
		const bool Bounded    = false;
		NODE *rval = BASE::template construct<ThreadSafe,Bounded>( k, arg );
		if ( rval )
			nFree_.fetch_sub( 1, memory_order_release );
		return rval;
	}


	void destruct(NODE *o)
	{
		const bool ThreadSafe = true;
		nFree_.fetch_add( 1, memory_order_release );
		BASE::template destruct<ThreadSafe>( o );
	}

	shared_ptr<NODE> alloc()
	{
		NODE *b = construct( CFreeListNodeKey<NODE>() );
		if ( ! b )
			throw InternalError("Unable to allocate Buffer");
		shared_ptr<NODE> rval = shared_ptr<NODE>( b, DTOR(this) );
		rval->setSelf( rval );
		return rval;
	}

	template <typename ARG> shared_ptr<NODE> alloc(ARG a)
	{
		NODE *b = construct( CFreeListNodeKey<NODE>(), a );
		if ( ! b )
			throw InternalError("Unable to allocate Buffer");
		shared_ptr<NODE> rval = shared_ptr<NODE>( b, DTOR(this) );
		rval->setSelf( rval );
		return rval;
	}
};

#endif
