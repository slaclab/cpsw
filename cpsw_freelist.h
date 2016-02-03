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

template <typename C> class CFreeListNodeAlloc : public std::allocator<C> {
private:
	atomic<unsigned int> *nAlloc_p_;
	atomic<unsigned int> *nFree_p_;
public:
	CFreeListNodeAlloc( atomic<unsigned int> *nAlloc_p, atomic<unsigned int> *nFree_p )
	:nAlloc_p_(nAlloc_p), nFree_p_(nFree_p)
	{
	}

	C *allocate(size_t n)
	{
		nAlloc_p_->fetch_add(n, memory_order_release);
#ifndef BOOST_LOCKFREE_FREELIST_INIT_RUNS_DTOR
		nFree_p_->fetch_add(n, memory_order_release);
#endif
		return std::allocator<C>::allocate( n );
	}
};

// NODE must be derived from CFreeListNode !
template <typename NODE>
class CFreeList : public freelist_stack< NODE, CFreeListNodeAlloc<NODE> >
{
private:
	atomic<unsigned int> nAlloc_;
	atomic<unsigned int> nFree_;

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
	CFreeList(int n = 0)
	:BASE( ALLOC( (nAlloc_=0, &nAlloc_ ), (nFree_ = 0, &nFree_) ) , n)
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

	NODE *construct(CFreeListNodeKey<NODE> k)
	{
		const bool ThreadSafe = true;
		const bool Bounded    = false;
		NODE *rval = BASE::template construct<ThreadSafe,Bounded>( k );
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
};

#endif
