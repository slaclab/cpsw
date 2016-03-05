#ifndef CPSW_ENUM_IMPL_BASE_H
#define CPSW_ENUM_IMPL_BASE_H

#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>

#include <vector>

using std::vector;

// for now we just use a vector (assuming there are
// no huge maps).
class CEnumImpl : public IMutableEnum, public vector<IEnum::Item> {
private:
	unsigned      nelms_;
	TransFormFunc xfrm_;
protected:
	virtual uint64_t transform(uint64_t in, bool isRead)
	{
		return xfrm_ ? xfrm_(in, isRead) : in;
	}
public:
	CEnumImpl()
	: nelms_(0)
	{
	}

	// implement the abstract iterator using
	// vector<>::iterator
	class CIteratorImpl : public IIterator {
		private:
			vector<Item>::const_iterator iter_;

			// allocate this iterator inside the IEnum::iterator object's 'rawmem'
			// area. IEnum::iterator is commonly allocated on the stack and using
			// placement new avoids using the heap for iterators...

			void *operator new(std::size_t sz, void *p, std::size_t rawsz)
			{
				if ( sz > rawsz )
					throw InternalError("CEnumImpl::CIteratorImpl raw memory area too small");
				return p;
			}

			friend class IEnum::iterator; // allow iterator to use this allocator

		public:
			CIteratorImpl( vector<Item>::const_iterator iter )
			: iter_(iter)
			{
			}

			virtual IIterator & operator++()
			{
				++iter_;
				return *this;
			}

			virtual Item        operator*()
			{
				return *iter_;
			}

			virtual bool operator==(IIterator *other)
			{
				return iter_ == static_cast<CIteratorImpl*>(other)->iter_;
			}
	
			virtual ~CIteratorImpl()
			{
			}
	};

	virtual IEnum::iterator begin() const;
	virtual IEnum::iterator end()   const;

	virtual unsigned        getNelms() const { return nelms_; }

	virtual Item map(uint64_t)      const;
	virtual Item map(const char*)   const;

	virtual void add(const char*, uint64_t);
};
#endif
