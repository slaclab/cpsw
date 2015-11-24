#ifndef CPSW_SVAL_H
#define CPSW_SVAL_H

#include <cpsw_api_builder.h>
#include <cpsw_entry.h>

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

class IEntryAdapt : public virtual IEntry {
protected:
	shared_ptr<const CEntryImpl> ie_;
	Path                         p_;

protected:
	IEntryAdapt(Path p, shared_ptr<const CEntryImpl> ie);

public:
	virtual const char *getName()        const { return ie_->getName(); }
	virtual const char *getDescription() const { return ie_->getDescription(); }
	virtual uint64_t    getSize()        const { return ie_->getSize(); }
	virtual Hub         isHub()          const { return ie_->isHub();   }
};

class CIntEntryImpl : public CEntryImpl, public virtual IIntField {
public:
	class CBuilder : public virtual IBuilder {
	private:
		std::string name_;
		uint64_t    sizeBits_;
		bool        isSigned_;
		int         lsBit_;	
		Mode        mode_;
        unsigned    wordSwap_;
		
		weak_ptr<CBuilder> self_;
	protected:
		CBuilder();
		template<typename AS> AS getSelf() { return static_pointer_cast<typename AS::element_type, CBuilder>( shared_ptr<CBuilder>(self_) ); }

		// all subclasses must implement
		virtual CBuilder *doClone() { return new CBuilder( *this ); }

		virtual void    setSelf(shared_ptr<CBuilder> in) { self_ = in; }
	public:
		virtual void    init();
		virtual Builder name(const char *);
		virtual Builder sizeBits(uint64_t);
		virtual Builder isSigned(bool);
		virtual Builder lsBit(int);
		virtual Builder mode(Mode);
		virtual Builder wordSwap(unsigned);
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
public:


	CIntEntryImpl(FKey k, uint64_t sizeBits = DFLT_SIZE_BITS, bool is_signed = DFLT_IS_SIGNED, int lsBit = DFLT_LS_BIT, Mode mode = DFLT_MODE, unsigned wordSwap = DFLT_WORD_SWAP);

	virtual CIntEntryImpl *clone(FKey k) { return new CIntEntryImpl( *this ); }
	virtual bool     isSigned()    const { return is_signed_; }
	virtual int      getLsBit()    const { return ls_bit_;    }
	virtual uint64_t getSizeBits() const { return size_bits_; }
	virtual unsigned getWordSwap() const { return wordSwap_;  }
	virtual Mode     getMode()     const { return mode_;      }
};

class IIntEntryAdapt : public IEntryAdapt, public virtual IScalVal_Base {
private:
	int nelms_;
public:
	IIntEntryAdapt(Path p, shared_ptr<const CIntEntryImpl> ie) : IEntryAdapt(p, ie), nelms_(-1) {}
	virtual bool     isSigned()    const { return asIntEntry()->isSigned();    }
	virtual int      getLsBit()    const { return asIntEntry()->getLsBit();    }
	virtual uint64_t getSizeBits() const { return asIntEntry()->getSizeBits(); }
	virtual unsigned getWordSwap() const { return asIntEntry()->getWordSwap(); }
	virtual IIntField::Mode     getMode()     const { return asIntEntry()->getMode(); }
	virtual unsigned getNelms();

protected:
	virtual shared_ptr<const CIntEntryImpl> asIntEntry() const { return static_pointer_cast<const CIntEntryImpl, const CEntryImpl>(ie_); }

};

class CScalVal_ROAdapt : public virtual IScalVal_RO, public virtual IIntEntryAdapt {
public:
	CScalVal_ROAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie)
	{
	}

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned);

	template <typename E> unsigned getVal(E *e, unsigned nelms) {
		return getVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E) );
	}

	virtual unsigned getVal(uint64_t *p, unsigned n) { return getVal<uint64_t>(p,n); }
	virtual unsigned getVal(uint32_t *p, unsigned n) { return getVal<uint32_t>(p,n); }
	virtual unsigned getVal(uint16_t *p, unsigned n) { return getVal<uint16_t>(p,n); }
	virtual unsigned getVal(uint8_t  *p, unsigned n) { return getVal<uint8_t> (p,n); }

};

class CScalVal_WOAdapt : public virtual IScalVal_WO, public virtual IIntEntryAdapt {
public:
	CScalVal_WOAdapt(Path p, shared_ptr<const CIntEntryImpl> ie);

	template <typename E> unsigned setVal(E *e, unsigned nelms) {
		return setVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E) );
	}

	virtual unsigned setVal(uint8_t  *, unsigned, unsigned);

	virtual unsigned setVal(uint64_t *p, unsigned n) { return setVal<uint64_t>(p,n); }
	virtual unsigned setVal(uint32_t *p, unsigned n) { return setVal<uint32_t>(p,n); }
	virtual unsigned setVal(uint16_t *p, unsigned n) { return setVal<uint16_t>(p,n); }
	virtual unsigned setVal(uint8_t  *p, unsigned n) { return setVal<uint8_t> (p,n); }

	virtual unsigned setVal(uint64_t  v);
};

class CScalVal_Adapt : public virtual CScalVal_ROAdapt, public virtual CScalVal_WOAdapt, public virtual IScalVal {
public:
	CScalVal_Adapt(Path p, shared_ptr<const CIntEntryImpl> ie);
};

#endif
