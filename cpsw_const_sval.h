 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_CONST_SVAL_H
#define CPSW_CONST_SVAL_H

#include <cpsw_sval.h>

class CConstIntEntryImpl;
typedef shared_ptr<CConstIntEntryImpl> ConstIntEntryImpl;

class CConstIntEntryAdapt;
typedef shared_ptr<CConstIntEntryAdapt> ConstIntEntryAdapt;


class CConstIntEntryImpl : public CIntEntryImpl {
public:
	static const bool DFLT_IS_DOUBLE = false;
private:
	bool               isDouble_;
	uint64_t           intVal_;
    double             doubleVal_;
public:
	CConstIntEntryImpl(Key &k, const char *name, bool isSigned = DFLT_IS_SIGNED, bool isDouble = DFLT_IS_DOUBLE, Enum enm = Enum());

	CConstIntEntryImpl(Key &k, YamlState &n);

	static  const char *_getClassName()       { return "ConstIntField"; }
	virtual const char * getClassName() const { return _getClassName(); }

	virtual void dumpYamlPart(YAML::Node &n) const;

	virtual YAML::Node dumpMyConfigToYaml(Path p)                  const;
	virtual void       loadMyConfigFromYaml(Path p, YAML::Node &n) const;

	CConstIntEntryImpl(const CConstIntEntryImpl &orig, Key &k)
	:CIntEntryImpl(orig, k),
	 isDouble_(orig.isDouble_),
	 intVal_(orig.intVal_ ),
	 doubleVal_(orig.doubleVal_)
	{
	}

	virtual CConstIntEntryImpl *clone(Key &k)   { return new CConstIntEntryImpl( *this, k ); }

	virtual bool     isDouble()       const;

	virtual bool     isString()       const;

	virtual double   getDouble()      const;
	virtual uint64_t getInt()         const;

	// setters can only be used from constructors
	virtual void     setIsDouble  (Key &k, bool     v);

	virtual EntryAdapt createAdapter(IEntryAdapterKey &, Path, const std::type_info&) const;
};

class CConstIntEntryAdapt : public CScalVal_ROAdapt {
public:
	CConstIntEntryAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie);

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned, IndexRange *r = 0 );

	virtual unsigned getVal(double   *p, unsigned n, IndexRange *r=0);
};

#endif