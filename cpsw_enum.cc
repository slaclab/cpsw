#include <cpsw_enum.h>

#include <stdarg.h>

#include <boost/make_shared.hpp>

using boost::make_shared;

IEnum::Item CEnumImpl::map(uint64_t x) const
{
vector<Item>::const_iterator i = vector<Item>::begin();
vector<Item>::const_iterator e = vector<Item>::end();

	while ( i != e ) {
		if ( (*i).second == x )
			return *i;
	}
	throw ConversionError("No enum found (num->enum)");
}

IEnum::Item CEnumImpl::map(const char *x) const
{
vector<Item>::const_iterator i = vector<Item>::begin();
vector<Item>::const_iterator e = vector<Item>::end();

	while ( i != e ) {
		if ( ! ::strcmp( (*i).first->c_str(), x ) )
			return *i;
	}
	throw ConversionError("No enum found (const char *->enum)");
}

IEnum::iterator::iterator(const IEnum *enm, bool atBeg)
{
const CEnumImpl *enmImpl = static_cast<const CEnumImpl *>(enm);

vector<Item>::const_iterator vec_it = atBeg ? enmImpl->vector<Item>::begin() : enmImpl->vector<Item>::end();
	// create an implementation of the abstract iterator (using vector<>::iterator)
	ifp = new (rawmem, sizeof(rawmem)) CEnumImpl::CIteratorImpl( vec_it );
}

IEnum::iterator::iterator(const IEnum::iterator &orig)
{
	ifp = new (rawmem, sizeof(rawmem)) CEnumImpl::CIteratorImpl( static_cast<CEnumImpl::CIteratorImpl*>(orig.ifp)->iter_ );
}

IEnum::iterator & IEnum::iterator::operator=(const IEnum::iterator &orig)
{
	ifp->~IIterator();
	ifp = new (rawmem, sizeof(rawmem)) CEnumImpl::CIteratorImpl( static_cast<CEnumImpl::CIteratorImpl*>(orig.ifp)->iter_ );
	return *this;
}

IEnum::iterator CEnumImpl::begin() const
{
	return IEnum::iterator(this, true);
}

IEnum::iterator CEnumImpl::end() const
{
	return IEnum::iterator(this, false);
}

void CEnumImpl::add(const char *str, uint64_t val)
{
string *s  = new string(str);
CString cs = CString(s);
	push_back( Item( cs, val ) );
}

MutableEnum IMutableEnum::create(TransformFunc f, ...)
{
va_list    ap;
const char *str;

shared_ptr<CEnumImpl> rval = make_shared<CEnumImpl>( f );

	va_start( ap, f );

	while ( (str = va_arg(ap, const char *)) ) {
		uint64_t num = va_arg(ap, uint64_t);
		rval->add( str, num );
	}

	return rval;
}

MutableEnum IMutableEnum::create(TransformFunc f)
{
	return create(f, NULL);
}

MutableEnum IMutableEnum::create()
{
	return create( NULL, NULL );
}

static uint64_t boolConv(uint64_t in, bool isRead)
{
	return isRead ? !!in : 
}

Enum const enumBool = IMutableEnum::create( boolConv, "False", 0, "True", 1, NULL );
