 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_const_sval.h>
#include <cpsw_yaml.h>

CConstIntEntryImpl::CConstIntEntryImpl(Key &k, const char *name, bool isSigned, Enum enm)
: CIntEntryImpl(k, name, 64, isSigned, 0, IVal_Base::RO, 0, enm),
  intVal_(0),
  doubleVal_(0.)
{
	// we only support full-fledged construction from YAML
}

bool
CConstIntEntryImpl::isString() const
{
	return IScalVal_Base::ASCII == getEncoding();
}

CConstIntEntryImpl::CConstIntEntryImpl(Key &k, YamlState &n)
: CIntEntryImpl(k, n)
{
Encoding     enc;
std::string  stringVal;
Enum         enm = getEnum();

	enc = getEncoding();

	if (   IScalVal_Base::NONE     != enc
	    && IScalVal_Base::ASCII    != enc
	    && IScalVal_Base::IEEE_754 != enc ) {
		throw InvalidArgError("CConstIntEntryImpl: Unsupported encoding");
	}

	setWordSwap(k,             0);
	setMode    (k, IVal_Base::RO);
	setLsBit   (k,             0);

	if ( ! readNode(n, YAML_KEY_value, &stringVal) ) {
		throw InvalidArgError("CConstIntEntryImpl: *must* have a 'value'");
	}

	if ( isString() ) {
		setSigned  (k, false);
		setSizeBits(k,     8);
		if ( enm ) {
			IEnum::Item item = enm->map( stringVal.c_str() );
			intVal_ = item.second;
		} else {
			intVal_ = 0;
		}
		doubleVal_ = intVal_;
	} else {
		setSizeBits(k,    64);

		if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
			readNode(n, YAML_KEY_value, &doubleVal_);
            setSigned(k, true);
			intVal_ = (int64_t)doubleVal_;
		} else {
			if ( isSigned() ) {
				int64_t v;
				readNode(n, YAML_KEY_value, &v);
				intVal_    = v;
				doubleVal_ = v;
			} else {
				readNode(n, YAML_KEY_value, &intVal_);
				doubleVal_ = intVal_;
			}
		}
		// if there is an enum - make sure it can map the value
		if ( enm )
			enm->map( intVal_ );
	}

	if ( ! enm && IScalVal_Base::IEEE_754 != enc ) {
		MutableEnum menm = IMutableEnum::create();
		menm->add( stringVal.c_str(), intVal_ );
		setEnum(k, menm);
	}
}

void
CConstIntEntryImpl::dumpYamlPart(YAML::Node &n) const
{
	if ( IScalVal_Base::IEEE_754 == getEncoding() )
		writeNode(n, YAML_KEY_value, doubleVal_);
	else
		writeNode(n, YAML_KEY_value, intVal_   );
	// If it was a string then string is saved as an enum
}

uint64_t
CConstIntEntryImpl::dumpMyConfigToYaml(Path p, YAML::Node &n) const
{
	// caller should ignore this node
	n = YAML::Node( YAML::NodeType::Undefined );
	return 0;
}

uint64_t
CConstIntEntryImpl::loadMyConfigFromYaml(Path p, YAML::Node &n) const
{
	throw ConfigurationError("This class doesn't implement 'loadMyConfigFromYaml'");
}

uint64_t
CConstIntEntryImpl::getInt() const
{
	return intVal_;
}

double
CConstIntEntryImpl::getDouble() const
{
	return doubleVal_;
}


EntryAdapt
CConstIntEntryImpl::createAdapter(IEntryAdapterKey &key, Path path, const std::type_info &interfaceType) const
{
	if ( isInterface<Val_Base>(interfaceType) || isInterface<ScalVal_Base>(interfaceType) ) {
		return _createAdapter< shared_ptr<IIntEntryAdapt> >( this, path );
	} else if ( isInterface<ScalVal_RO>(interfaceType) ) {
		if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
			throw InterfaceNotImplementedError("ScalVal_RO not available for IEEE_754 encoding");
		}
		return _createAdapter<ConstIntEntryAdapt>( this, path );
	} else if ( isInterface<DoubleVal_RO>(interfaceType) ) {
		return _createAdapter<ConstDblEntryAdapt>(this, path);
	}
	throw InterfaceNotImplementedError("CConstIntEntryImpl does not implement requested interface");
}

CConstIntEntryAdapt::CConstIntEntryAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
: IIntEntryAdapt(k, p, ie),
  CScalVal_ROAdapt(k, p, ie)
{
}

CConstDblEntryAdapt::CConstDblEntryAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
: IIntEntryAdapt(k, p, ie),
  CDoubleVal_ROAdapt(k, p, ie)
{
}


unsigned
CConstIntEntryAdapt::getVal(uint8_t  *buf, unsigned nelms, unsigned elsz, IndexRange *r)
{
SlicedPathIterator it( p_, r );
unsigned nelmsOnPath = it.getNelmsLeft();
ByteOrder hostEndian = hostByteOrder();

shared_ptr<const CConstIntEntryImpl> ie = static_pointer_cast<const CConstIntEntryImpl>( ie_ );
uint64_t val = ie->getInt();

	if ( nelms >= nelmsOnPath )
		nelms = nelmsOnPath;
	else
		throw InvalidArgError("Invalid Argument: buffer too small");
	
	uint8_t *valp = reinterpret_cast<uint8_t*>( &val );

	if ( BE == hostEndian ) {
 		valp += sizeof(uint64_t) - elsz;
	} if ( LE != hostEndian ) {
		throw InternalError("Unknown host endian-ness");
	}

	for (unsigned n = 0; n < nelms; n++ ) {
		memcpy( buf, valp, elsz );
		buf += elsz;
	}

	return nelms;
}

unsigned
CConstDblEntryAdapt::getVal(double   *buf, unsigned nelms, IndexRange *r)
{
SlicedPathIterator it( p_, r );
unsigned nelmsOnPath = it.getNelmsLeft();

shared_ptr<const CConstIntEntryImpl> ie = static_pointer_cast<const CConstIntEntryImpl>( ie_ );

double doubleVal = ie->getDouble();

	if ( nelms >= nelmsOnPath )
		nelms = nelmsOnPath;
	else
		throw InvalidArgError("Invalid Argument: buffer too small");

	for ( unsigned n = 0; n < nelms; n++ ) {
		buf[n] = doubleVal;
	}

	return nelms;
}
