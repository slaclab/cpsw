#ifndef CPSW_ENUM_IMPL_BASE_H
#define CPSW_ENUM_IMPL_BASE_H

#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>

#include <vector>

#include <cpsw_yaml.h>

using std::vector;

// for now we just use a vector (assuming there are
// no huge maps).
class CEnumImpl : public IMutableEnum, public vector<IEnum::Item>, public CYamlSupportBase {
private:
	unsigned      nelms_;
	TransformFunc xfrm_;

protected:
	virtual uint64_t transform(uint64_t in)
	{
		return xfrm_ ? xfrm_->xfrm(in) : in;
	}

	CEnumImpl(TransformFunc xfrm);

	friend class CTransformFuncImpl;

public:
	// For an example check class 'CBoolTransFormFunc' in cpsw_enum.cc

	class CTransformFuncImpl : public CTransformFunc, public IYamlFactoryBase<MutableEnum> {
	public:
		// subclass must define this method, returning a unique name
		static const char *getName_() { return "?"; }

		// subclass must define a constructor and pass on the key
		CTransformFuncImpl(const Key &k);

		// subclass implements it's own conversion
		virtual uint64_t xfrm(uint64_t in)
		{
			return in;
		}

		virtual MutableEnum makeItem(YamlState &node, IYamlTypeRegistry<MutableEnum> *r);

		// singleton access via base-class template:
		//
		//  TransformFunc CTransformFunc::get<Subclass>()
	};

	virtual void dumpYamlPart(YAML::Node &node) const;

	virtual void setClassName(YAML::Node &node) const;

	virtual const char *getClassName() const
	{
		return xfrm_->getName();
	}

	virtual ~CEnumImpl();

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

			CIteratorImpl()
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

	virtual Item map(uint64_t)     ;
	virtual Item map(const char*)  ;

	virtual void add(const char*, uint64_t);

	// converter that returns !!input
	static TransformFunc uint64ToBool();

	static MutableEnum create(TransformFunc f);
};
#endif
