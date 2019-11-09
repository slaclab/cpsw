 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_enum.h>
#include <cpsw_obj_cnt.h>

#include <stdio.h>

#include <stdarg.h>
#include <string>

using std::string;

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

template<> void
CYamlTypeRegistry<MutableEnum>::extractClassName(std::vector<std::string> *svec_p, YamlState &node)
{
	svec_p->push_back( node.Tag() );
}

template<> bool
CYamlTypeRegistry<MutableEnum>::extractInstantiate(YamlState &node)
{
	return true;
}

static CEnumImpl::Registry getEnumRegistry()
{
static CEnumImpl::Registry the_registry_( new CYamlTypeRegistry<MutableEnum> );
	return the_registry_;
}

MutableEnum IMutableEnum::create(YamlState &node)
{
	return getEnumRegistry()->makeItem( node );
}


CEnumImpl::CTransformFuncImpl::CTransformFuncImpl(const Key &key)
: CTransformFunc( key ),
  IYamlFactoryBase<MutableEnum>( getName(), getEnumRegistry() )
{
}

MutableEnum
CEnumImpl::CTransformFuncImpl::makeItem(YamlState &node)
{
MutableEnum enm = IMutableEnum::create(this, 0);
unsigned    i;
	for ( i=0; i<node.size(); i++ ) {
		YamlState   node_item( &node, i );
		std::string nam;
		uint64_t    val;

		mustReadNode(node_item, YAML_KEY_name,  &nam);
		mustReadNode(node_item, YAML_KEY_value, &val);
		enm->add( nam.c_str(), val );
	}
	return enm;
}

void
CEnumImpl::dumpYamlPart(YAML::Node &node) const
{
	IEnum::iterator it( begin() );

	while ( it != end() ) {
		YAML::Node item;
		writeNode(item, YAML_KEY_name, *(*it).first );
		writeNode(item, YAML_KEY_value, (*it).second); 
		pushNode( node, 0, item );
		++it;
	}
}

void
CEnumImpl::setClassName(YAML::Node &node) const
{
	// don't set tag if this is the default function
	if ( xfrm_ != CTransformFunc::get<CTransformFuncImpl>() ) {
		// SetTag prepends a '!'
		node.SetTag( getClassName() );
	}
}

IMutableEnum::CTransformFunc::CTransformFunc(const Key &key)
:name_( key.tag_ )
{
}

IMutableEnum::CTransformFunc::~CTransformFunc()
{
}

class CBoolTransformFunc : public CEnumImpl::CTransformFuncImpl {
public:
	static const char *getName_()
	{
		return "uint64ToBool";
	}

	CBoolTransformFunc(const Key &key)
	: CEnumImpl::CTransformFuncImpl(key)
	{
	}

	virtual uint64_t xfrm(uint64_t in)
	{
		return in ? 1 : 0;
	}
};

IMutableEnum::TransformFunc
CEnumImpl::uint64ToBool()
{
	return IMutableEnum::CTransformFunc::get<CBoolTransformFunc>();
}

CEnumImpl::CEnumImpl(TransformFunc xfrm)
: nelms_(0),
  xfrm_(xfrm ? xfrm : IMutableEnum::CTransformFunc::get<CTransformFuncImpl>())
{
	++ocnt();
}

CEnumImpl::~CEnumImpl()
{
	--ocnt();
}

IMutableEnum::TransformFunc __defaultFunc = IMutableEnum::CTransformFunc::get<CEnumImpl::CTransformFuncImpl>();
IMutableEnum::TransformFunc __boolFunc    = IMutableEnum::CTransformFunc::get<CBoolTransformFunc>();
Enum const enumBool  = IMutableEnum::create( CEnumImpl::uint64ToBool(), "False", 0, "True", 1, NULL );
Enum const enumYesNo = IMutableEnum::create( CEnumImpl::uint64ToBool(), "No",    0, "Yes",  1, NULL );
