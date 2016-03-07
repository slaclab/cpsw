#include <cpsw_enum.h>
#include <cpsw_obj_cnt.h>

#include <stdio.h>

#include <stdarg.h>

static DECLARE_OBJ_COUNTER( ocnt, "Enum", 2 ) // bool + yesno

IEnum::Item CEnumImpl::map(uint64_t x)
{
vector<Item>::const_iterator i = vector<Item>::begin();
vector<Item>::const_iterator e = vector<Item>::end();

	x = transform( x );

	while ( i != e ) {
		if ( (*i).second == x )
			return *i;
		++i;
	}
	throw ConversionError("No enum found (num->enum)");
}

IEnum::Item CEnumImpl::map(const char *x)
{
vector<Item>::const_iterator i = vector<Item>::begin();
vector<Item>::const_iterator e = vector<Item>::end();

	while ( i != e ) {
		if ( ! ::strcmp( (*i).first->c_str(), x ) )
			return *i;
		++i;
	}
	throw ConversionError("No enum found (const char *->enum)");
}

IEnum::iterator::iterator()
{
	ifp = new (rawmem, sizeof(rawmem)) CEnumImpl::CIteratorImpl();
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
	nelms_++;
}

MutableEnum CEnumImpl::create(TransformFunc f)
{
CEnumImpl *enm_p = new CEnumImpl( f );
	return MutableEnum(enm_p);
}

MutableEnum IMutableEnum::create(TransformFunc f, ...)
{
va_list     ap;
const char *str;
MutableEnum enm = CEnumImpl::create( f );

	va_start(ap, f);

	while ( (str = va_arg( ap, const char * )) ) {

		int num = va_arg( ap, int );

		enm->add( str, (uint64_t)(int64_t)num );
	}

	va_end(ap);

	return enm;
}

MutableEnum IMutableEnum::create(TransformFunc f)
{
	return create(f, NULL);
}

MutableEnum IMutableEnum::create()
{
	return create( NULL, NULL );
}

static uint64_t boolConv(uint64_t in)
{
	return in ? 1 : 0;
}

CEnumImpl::CEnumImpl(TransformFunc xfrm)
: nelms_(0),
  xfrm_(xfrm)
{
	++ocnt();
}

CEnumImpl::~CEnumImpl()
{
	--ocnt();
}

Enum const enumBool  = IMutableEnum::create( boolConv, "False", 0, "True", 1, NULL );
Enum const enumYesNo = IMutableEnum::create( boolConv, "No",    0, "Yes",  1, NULL );
