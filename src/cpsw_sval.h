 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_SVAL_H
#define CPSW_SVAL_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>

using cpsw::static_pointer_cast;
using cpsw::weak_ptr;

class CIntEntryImpl;
typedef shared_ptr<CIntEntryImpl> IntEntryImpl;

class CScalVal_ROAdapt;
typedef shared_ptr<CScalVal_ROAdapt>   ScalVal_ROAdapt;
class CScalVal_WOAdapt;
typedef shared_ptr<CScalVal_WOAdapt>   ScalVal_WOAdapt;
class CScalValAdapt;
typedef shared_ptr<CScalValAdapt>      ScalValAdapt;
class CDoubleVal_ROAdapt;
typedef shared_ptr<CDoubleVal_ROAdapt> DoubleVal_ROAdapt;
class CDoubleVal_WOAdapt;
typedef shared_ptr<CDoubleVal_WOAdapt> DoubleVal_WOAdapt;
class CDoubleValAdapt;
typedef shared_ptr<CDoubleValAdapt>    DoubleValAdapt;



class CIntEntryImpl : public CEntryImpl, public virtual IIntField {
public:
	static const int DFLT_CONFIG_PRIO_RW = 1;
	static const int DFLT_CONFIG_BASE    = 16; // dump configuration values in hex

	typedef IScalVal_Base::Encoding Encoding;

	static const Encoding DFLT_ENCODING = IScalVal::NONE;

	class CBuilder : public virtual IBuilder, public CShObj {
	private:
		std::string name_;
		uint64_t           sizeBits_;
		bool               isSigned_;
		int                lsBit_;
		Mode               mode_;
		int                configPrio_;
		int                configBase_;
		unsigned           wordSwap_;
		Encoding           encoding_;
		Enum               enum_;

	protected:
		typedef shared_ptr<CBuilder> BuilderImpl;

	public:
		CBuilder(Key &k);
		CBuilder(CBuilder &orig, Key &k)
		:CShObj     (orig, k),
		 name_      (orig.name_),
		 sizeBits_  (orig.sizeBits_),
		 isSigned_  (orig.isSigned_),
		 lsBit_     (orig.lsBit_),
		 mode_      (orig.mode_),
		 configPrio_(orig.configPrio_),
		 configBase_(orig.configBase_),
		 wordSwap_  (orig.wordSwap_),
		 encoding_  (orig.encoding_),
		 enum_      (orig.enum_)
		{
		}

		// all subclasses must implement
		virtual CBuilder *clone(Key &k) { return new CBuilder( *this, k); }

		virtual void    init();
		virtual Builder name(const char *);
		virtual Builder sizeBits(uint64_t);
		virtual Builder isSigned(bool);
		virtual Builder lsBit(int);
		virtual Builder mode(Mode);
		virtual Builder configPrio(int);
		virtual Builder configBase(int);
		virtual Builder wordSwap(unsigned);
		virtual Builder encoding(Encoding);
		virtual Builder setEnum(Enum);
		virtual Builder reset();

		virtual IntField build();
		virtual IntField build(const char*);

		virtual Builder clone();

		static  Builder create();
	};
private:
	bool               isSigned_;
	int                lsBit_;
	uint64_t           sizeBits_;
	Mode               mode_;
	unsigned           wordSwap_;
	Encoding           encoding_;
	Enum               enum_;
	int                configBase_;

	void checkArgs();

protected:

	virtual int      getDefaultConfigPrio() const;

public:

	CIntEntryImpl(Key &k, const char *name, uint64_t sizeBits = DFLT_SIZE_BITS, bool isSigned = DFLT_IS_SIGNED, int lsBit = DFLT_LS_BIT, Mode mode = DFLT_MODE, unsigned wordSwap = DFLT_WORD_SWAP, Enum enm = Enum());

	CIntEntryImpl(Key &k, YamlState &n);

	static  const char *_getClassName()       { return "IntField";      }
	virtual const char * getClassName() const { return _getClassName(); }

	virtual void dumpYamlPart(YAML::Node &n) const;

	virtual uint64_t dumpMyConfigToYaml(Path p, YAML::Node &n) const;
	virtual uint64_t loadMyConfigFromYaml(Path p, YAML::Node &n) const;

	CIntEntryImpl(const CIntEntryImpl &orig, Key &k)
	:CEntryImpl(orig, k),
	 isSigned_(orig.isSigned_),
	 lsBit_(orig.lsBit_),
	 sizeBits_(orig.sizeBits_),
	 mode_(orig.mode_),
	 wordSwap_(orig.wordSwap_),
	 encoding_(orig.encoding_),
	 enum_(orig.enum_),
	 configBase_(orig.configBase_)
	{
	}

	virtual CIntEntryImpl *clone(Key &k)   { return new CIntEntryImpl( *this, k ); }

	virtual bool     isSigned()      const;
	virtual int      getLsBit()      const;
	virtual uint64_t getSizeBits()   const;
	virtual unsigned getWordSwap()   const;
	virtual Encoding getEncoding()   const;
	virtual int      getConfigBase() const;
	virtual Mode     getMode()       const;
	virtual Enum     getEnum()       const;

	// setters can only be used from constructors
	virtual void     setSigned    (Key &k, bool     v);
	virtual void     setLsBit     (Key &k, int      v);
	virtual void     setSizeBits  (Key &k, uint64_t v);
	virtual void     setWordSwap  (Key &k, unsigned v);
	virtual void     setMode      (Key &k, Mode     v);
	virtual void     setEnum      (Key &k, Enum     v);

	virtual void     setConfigBase(int);
	virtual void     setEncoding(Encoding);

	virtual void postHook( ConstShObj );

	virtual EntryAdapt createAdapter(IEntryAdapterKey &, ConstPath, const std::type_info&) const;
};

class IIntEntryAdapt : public IEntryAdapt, public virtual IScalVal_Base {
private:
	int nelms_;
public:
	IIntEntryAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);
	virtual bool     isSigned()    const { return asIntEntry()->isSigned();    }
	virtual int      getLsBit()    const { return asIntEntry()->getLsBit();    }
	virtual uint64_t getSizeBits() const { return asIntEntry()->getSizeBits(); }
	virtual unsigned getWordSwap() const { return asIntEntry()->getWordSwap(); }
	virtual Encoding getEncoding() const { return asIntEntry()->getEncoding(); }
	virtual Mode     getMode()     const { return asIntEntry()->getMode(); }

	virtual Enum     getEnum()     const { return asIntEntry()->getEnum(); }

	virtual unsigned nelmsFromIdx(IndexRange *r);

	virtual ~IIntEntryAdapt();

protected:
	virtual shared_ptr<const CIntEntryImpl> asIntEntry() const { return static_pointer_cast<const CIntEntryImpl, const CEntryImpl>(ie_); }

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned, SlicedPathIterator *it);
	virtual unsigned getVal(AsyncIO aio, uint8_t  *, unsigned, unsigned, SlicedPathIterator *it);

	template <typename E> unsigned getVal(E *e, unsigned nelms, IndexRange *r)
	{
	SlicedPathIterator it(p_, r);
		try {
			return getVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), &it );
		} catch (const IOError &ex)  {
			throw IOError((ex.what() + std::string(": ") + p_->toString()).c_str());
		}
	}

	template <typename E> unsigned getVal(AsyncIO aio, E *e, unsigned nelms, IndexRange *r)
	{
	SlicedPathIterator it(p_, r);
		try {
			return getVal(aio, reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), &it );
		} catch (const IOError &ex)  {
			throw IOError((ex.what() + std::string(": ") + p_->toString()).c_str());
		}			
	}

	template <typename E> unsigned getVal(E *e, unsigned nelms, SlicedPathIterator *it)
	{
		try {
			return getVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), it );
		} catch (const IOError &ex)  {
			throw IOError((ex.what() + std::string(": ") + p_->toString()).c_str());
		}		
	}

	template <typename E> unsigned getVal(AsyncIO aio, E *e, unsigned nelms, SlicedPathIterator *it)
	{
		try {
			return getVal(aio, reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), it );
		} catch (const IOError &ex)  {
			throw IOError((ex.what() + std::string(": ") + p_->toString()).c_str());
		}		
	}


	virtual unsigned checkNelms(unsigned nelms, SlicedPathIterator *it);

	virtual unsigned setVal(uint8_t  *, unsigned, unsigned, IndexRange *r = 0);

	template <typename E> unsigned setVal(E *e, unsigned nelms, IndexRange *r)
	{
		return setVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), r );
	}

};

class CScalVal_ROAdapt : public virtual IScalVal_RO, public virtual IIntEntryAdapt {
public:
	CScalVal_ROAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual unsigned getVal(uint64_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint64_t>(p,n,r);
	}

	virtual unsigned getVal(AsyncIO aio, uint64_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint64_t>(aio, p,n,r);
	}

	virtual unsigned getVal(uint32_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint32_t>(p,n,r);
	}

	virtual unsigned getVal(AsyncIO aio, uint32_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint32_t>(aio, p,n,r);
	}

	virtual unsigned getVal(uint16_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint16_t>(p,n,r);
	}

	virtual unsigned getVal(AsyncIO aio, uint16_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint16_t>(aio, p,n,r);
	}

	virtual unsigned getVal(uint8_t  *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint8_t> (p,n,r);
	}

	virtual unsigned getVal(AsyncIO aio, uint8_t *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::getVal<uint8_t>(aio, p,n,r);
	}

	virtual unsigned getVal(CString  *p, unsigned n, IndexRange *r=0);
	virtual unsigned getVal(AsyncIO aio, CString  *p, unsigned n, IndexRange *r=0);

protected:
	using IIntEntryAdapt::getVal;
};

class CDoubleVal_ROAdapt : public virtual IDoubleVal_RO, public virtual IIntEntryAdapt {
public:
	CDoubleVal_ROAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual void     int2dbl(double *dst, uint64_t *src, unsigned n);
	virtual void     dbl2dbl(double *dst, unsigned n);

	virtual unsigned getVal(double   *p, unsigned n, IndexRange *r=0);
	virtual unsigned getVal(AsyncIO aio, double   *p, unsigned n, IndexRange *r=0);
};

class CScalVal_WOAdapt : public virtual IScalVal_WO, public virtual IIntEntryAdapt {
public:
	CScalVal_WOAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual unsigned setVal(uint64_t    *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::setVal<uint64_t>(p,n,r);
	}

	virtual unsigned setVal(uint32_t    *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::setVal<uint32_t>(p,n,r);
	}

	virtual unsigned setVal(uint16_t    *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::setVal<uint16_t>(p,n,r);
	}

	virtual unsigned setVal(uint8_t     *p, unsigned n, IndexRange *r=0)
	{
		return IIntEntryAdapt::setVal<uint8_t> (p,n,r);
	}


	virtual unsigned setVal(const char* *p, unsigned n, IndexRange *r=0);

	virtual unsigned setVal(uint64_t     v, IndexRange *r=0);
	virtual unsigned setVal(const char*  v, IndexRange *r=0);

};

class CDoubleVal_WOAdapt : public virtual IDoubleVal_WO, public virtual IIntEntryAdapt {
public:
	CDoubleVal_WOAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);

	virtual void     dbl2int(uint64_t *dst, double *src, unsigned n);
	virtual void     dbl2dbl(double *dst, unsigned n);

	virtual unsigned setVal(double      *p, unsigned n, IndexRange *r=0);
	virtual unsigned setVal(double       v, IndexRange *r=0);
};


class CScalValAdapt : public virtual CScalVal_ROAdapt, public virtual CScalVal_WOAdapt, public virtual IScalVal {
public:
	CScalValAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);
};

class CDoubleValAdapt : public virtual CDoubleVal_ROAdapt, public virtual CDoubleVal_WOAdapt, public virtual IDoubleVal {
public:
	CDoubleValAdapt(Key &k, ConstPath p, shared_ptr<const CIntEntryImpl> ie);
};


#endif
