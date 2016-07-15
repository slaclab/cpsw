#ifndef CPSW_SVAL_H
#define CPSW_SVAL_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>

using boost::static_pointer_cast;
using boost::weak_ptr;

class CIntEntryImpl;
typedef shared_ptr<CIntEntryImpl> IntEntryImpl;

class CScalVal_ROAdapt;
typedef shared_ptr<CScalVal_ROAdapt> ScalVal_ROAdapt;
class CScalVal_WOAdapt;
typedef shared_ptr<CScalVal_WOAdapt> ScalVal_WOAdapt;
class CScalVal_Adapt;
typedef shared_ptr<CScalVal_Adapt>   ScalVal_Adapt;


class CIntEntryImpl : public CEntryImpl, public virtual IIntField {
public:
	class CBuilder : public virtual IBuilder, public CShObj {
	private:
		std::string name_;
		uint64_t    sizeBits_;
		bool        isSigned_;
		int         lsBit_;	
		Mode        mode_;
        unsigned    wordSwap_;
		Enum        enum_;

	protected:
		typedef shared_ptr<CBuilder> BuilderImpl;
		
	public:
		CBuilder(Key &k);
		CBuilder(CBuilder &orig, Key &k)
		:CShObj    (orig, k),
		 name_     (orig.name_),
		 sizeBits_ (orig.sizeBits_),
		 isSigned_ (orig.isSigned_),
		 lsBit_    (orig.lsBit_),
		 mode_     (orig.mode_),
         wordSwap_ (orig.wordSwap_),
         enum_     (orig.enum_)
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
		virtual Builder wordSwap(unsigned);
		virtual Builder setEnum(Enum);
		virtual Builder reset();

		virtual IntField build();
		virtual IntField build(const char*);

		virtual Builder clone();

		static  Builder create();
	};
private:
	bool     is_signed_;
	int      ls_bit_;
	uint64_t size_bits_;
	Mode     mode_;
	unsigned wordSwap_;
	Enum     enum_;

	void checkArgs();

public:


	CIntEntryImpl(Key &k, const char *name, uint64_t sizeBits = DFLT_SIZE_BITS, bool is_signed = DFLT_IS_SIGNED, int lsBit = DFLT_LS_BIT, Mode mode = DFLT_MODE, unsigned wordSwap = DFLT_WORD_SWAP, Enum enm = Enum());

	CIntEntryImpl(Key &k, const YAML::Node &n);

	static  const char *_getClassName()       { return "IntField";      }
	virtual const char * getClassName() const { return _getClassName(); }

	virtual void dumpYamlPart(YAML::Node &n) const;
	
	virtual void dumpYamlConfig(YAML::Node &, Path p) const;

	virtual void loadYamlConfig(const YAML::Node &, Path p) const;

	CIntEntryImpl(CIntEntryImpl &orig, Key &k)
	:CEntryImpl(orig, k),
	 is_signed_(orig.is_signed_),
	 ls_bit_(orig.ls_bit_),
	 size_bits_(orig.size_bits_),
	 mode_(orig.mode_),
	 wordSwap_(orig.wordSwap_),
	 enum_(orig.enum_)
	{
	}

	virtual CIntEntryImpl *clone(Key &k) { return new CIntEntryImpl( *this, k ); }
	virtual bool     isSigned()    const { return is_signed_; }
	virtual int      getLsBit()    const { return ls_bit_;    }
	virtual uint64_t getSizeBits() const { return size_bits_; }
	virtual unsigned getWordSwap() const { return wordSwap_;  }
	virtual Mode     getMode()     const { return mode_;      }
	virtual Enum     getEnum()     const { return enum_;      }
};

class IIntEntryAdapt : public IEntryAdapt, public virtual IScalVal_Base {
private:
	int nelms_;
public:
	IIntEntryAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie) : IEntryAdapt(k, p, ie), nelms_(-1) {}
	virtual bool     isSigned()    const { return asIntEntry()->isSigned();    }
	virtual int      getLsBit()    const { return asIntEntry()->getLsBit();    }
	virtual uint64_t getSizeBits() const { return asIntEntry()->getSizeBits(); }
	virtual unsigned getWordSwap() const { return asIntEntry()->getWordSwap(); }
	virtual IIntField::Mode     getMode()     const { return asIntEntry()->getMode(); }
	virtual Path     getPath()     const { return IEntryAdapt::getPath(); }
	virtual unsigned getNelms();

	virtual Enum     getEnum()     const { return asIntEntry()->getEnum(); }

protected:
	virtual shared_ptr<const CIntEntryImpl> asIntEntry() const { return static_pointer_cast<const CIntEntryImpl, const CEntryImpl>(ie_); }

};

class CScalVal_ROAdapt : public virtual IScalVal_RO, public virtual IIntEntryAdapt {
public:
	CScalVal_ROAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
	{
	}

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned, IndexRange *r = 0 );

	template <typename E> unsigned getVal(E *e, unsigned nelms, IndexRange *r) {
		return getVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), r );
	}

	virtual unsigned getVal(uint64_t *p, unsigned n, IndexRange *r=0) { return getVal<uint64_t>(p,n,r); }
	virtual unsigned getVal(uint32_t *p, unsigned n, IndexRange *r=0) { return getVal<uint32_t>(p,n,r); }
	virtual unsigned getVal(uint16_t *p, unsigned n, IndexRange *r=0) { return getVal<uint16_t>(p,n,r); }
	virtual unsigned getVal(uint8_t  *p, unsigned n, IndexRange *r=0) { return getVal<uint8_t> (p,n,r); }
	virtual unsigned getVal(CString  *p, unsigned n, IndexRange *r=0);

	static ScalVal_ROAdapt create(Path);
};

class CScalVal_WOAdapt : public virtual IScalVal_WO, public virtual IIntEntryAdapt {
public:
	CScalVal_WOAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie);

	template <typename E> unsigned setVal(E *e, unsigned nelms, IndexRange *r) {
		return setVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E), r );
	}

	virtual unsigned setVal(uint8_t  *, unsigned, unsigned, IndexRange *r = 0);

	virtual unsigned setVal(uint64_t    *p, unsigned n, IndexRange *r=0) { return setVal<uint64_t>(p,n,r); }
	virtual unsigned setVal(uint32_t    *p, unsigned n, IndexRange *r=0) { return setVal<uint32_t>(p,n,r); }
	virtual unsigned setVal(uint16_t    *p, unsigned n, IndexRange *r=0) { return setVal<uint16_t>(p,n,r); }
	virtual unsigned setVal(uint8_t     *p, unsigned n, IndexRange *r=0) { return setVal<uint8_t> (p,n,r); }
	virtual unsigned setVal(const char* *p, unsigned n, IndexRange *r=0);

	virtual unsigned setVal(uint64_t     v, IndexRange *r=0);
	virtual unsigned setVal(const char*  v, IndexRange *r=0);

	static ScalVal_WOAdapt create(Path);
};

class CScalVal_Adapt : public virtual CScalVal_ROAdapt, public virtual CScalVal_WOAdapt, public virtual IScalVal {
public:
	CScalVal_Adapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie);
	static ScalVal_Adapt create(Path);
};

#endif
