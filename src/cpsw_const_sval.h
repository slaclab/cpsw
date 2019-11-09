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

class CConstDblEntryAdapt;
typedef shared_ptr<CConstDblEntryAdapt> ConstDblEntryAdapt;

class CConstIntEntryImpl : public CIntEntryImpl {
public:
	static const bool DFLT_IS_DOUBLE = false;
private:
	uint64_t           intVal_;
    double             doubleVal_;
public:
	CConstIntEntryImpl(Key &k, const char *name, bool isSigned = DFLT_IS_SIGNED, Enum enm = Enum());

	CConstIntEntryImpl(Key &k, YamlState &n);

	static  const char *_getClassName()       { return "ConstIntField"; }
	virtual const char * getClassName() const { return _getClassName(); }

	virtual void dumpYamlPart(YAML::Node &n) const;

	virtual uint64_t dumpMyConfigToYaml(Path p, YAML::Node &n) const;
	virtual uint64_t loadMyConfigFromYaml(Path p, YAML::Node &n) const;

	CConstIntEntryImpl(const CConstIntEntryImpl &orig, Key &k)
	:CIntEntryImpl(orig, k),
	 intVal_(orig.intVal_ ),
	 doubleVal_(orig.doubleVal_)
	{
	}

	virtual CConstIntEntryImpl *clone(Key &k)   { return new CConstIntEntryImpl( *this, k ); }

	virtual bool     isString()       const;

	virtual double   getDouble()      const;
	virtual uint64_t getInt()         const;

	virtual EntryAdapt createAdapter(IEntryAdapterKey &, ConstPath, const std::type_info&) const;
};

class CConstIntEntryAdapt : public virtual CScalVal_ROAdapt {
public:
	CConstIntEntryAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned, SlicedPathIterator *it);
	virtual unsigned getVal(AsyncIO aio, uint8_t  *, unsigned, unsigned, SlicedPathIterator *it);
};

class CConstDblEntryAdapt : public virtual CDoubleVal_ROAdapt {
public:
	CConstDblEntryAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual unsigned getVal(double   *p, unsigned n, IndexRange *r);
	virtual unsigned getVal(AsyncIO aio, double   *p, unsigned n, IndexRange *r);
};

#endif
