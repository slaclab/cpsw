 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_sval.h>
#include <cpsw_address.h>
#include <cpsw_async_io.h>

#include <string>

#include <cpsw_yaml.h>

//#define SVAL_DEBUG

using std::string;

const int CIntEntryImpl::DFLT_CONFIG_PRIO_RW;

/* Must make this a macro so that the on-stack array is within the user's
 * stack frame!
 */
#define TMP_BUF_DECL(EL, name, nelms) \
    EL         name##_stackBuf[TmpBuf<EL>::onStack(nelms)]; \
    TmpBuf<EL> name( name##_stackBuf, nelms );

template <typename EL> class TmpBuf {
private:
    size_t     nelms_;
	bool       onStack_;
	EL        *buff_;

	static const size_t STACK_BREAK = 4096*10;

public:

	static size_t onStack(size_t nelms)
	{
		return nelms * sizeof(EL) < STACK_BREAK ? nelms : 0;
	}

	size_t getNelms() const
	{
		return nelms_;
	}

	EL *getBufp()
	{
		return buff_;
	}

	EL & operator[](size_t i)
	{
		return buff_[i];
	}

	~TmpBuf()
	{
		if ( ! onStack_ && buff_ )
			delete [] buff_;
	}

	TmpBuf(EL *stackBuf, size_t nelms)
	: nelms_( nelms ),
	  onStack_ ( !!onStack(nelms) ),
	  buff_( onStack_ ? stackBuf : new EL[nelms_] )
	{
	}
};

/* Must make this a macro so that the on-stack array is within the user's
 * stack frame!
 */
#define VALS_BUF_DECL(EL, VL, name, nelms) \
    EL          name##_stackBuf[TmpBuf<EL>::onStack(nelms)]; \
    Vals<EL,VL> name( name##_stackBuf, nelms );

template <typename EL, typename VL> class Vals : public TmpBuf<EL> {
public:
	Vals(EL *stackBuf, unsigned nelms)
	: TmpBuf<EL>( stackBuf, nelms )
	{
	}

	unsigned setVal(CScalVal_WOAdapt *scalValAdapt, IndexRange *r, VL v)
	{
		for ( unsigned i = 0; i< TmpBuf<EL>::getNelms(); i++ )
			(*this)[i] = (EL)v;
		return scalValAdapt->setVal( TmpBuf<EL>::getBufp(), TmpBuf<EL>::getNelms(), r );
	}

	unsigned getVal(CScalVal_ROAdapt *scalValAdapt, VL *v_p, IndexRange *r);
	unsigned setVal(CScalVal_WOAdapt *scalValAdapt, IndexRange *r, VL *v_p);

	~Vals()
	{
	}
};

static uint64_t b2B(uint64_t bits)
{
	return (bits + 7)/8;
}

static uint64_t
computeSize(unsigned wordSwap, uint64_t sizeBits, int lsBit)
{
	return	wordSwap > 0 && wordSwap != b2B(sizeBits) ? b2B(sizeBits) + (lsBit ? 1 : 0) : b2B(sizeBits + lsBit);
}

static int checkConfigBase(int proposed)
{
	if ( 0 == proposed )
		proposed = CIntEntryImpl::DFLT_CONFIG_BASE;
	if ( 10 != proposed && 16 != proposed )
		throw InvalidArgError("configBase must be 10 or 16");
	return proposed;
}

void
CIntEntryImpl::checkArgs()
{
unsigned byteSize = b2B(sizeBits_);

	if ( lsBit_ > 7 ) {
		throw InvalidArgError("lsBit (bit shift) must be in 0..7");
	}

	if ( wordSwap_ == byteSize )
		wordSwap_ = 0;

	if ( wordSwap_ > 0 ) {
		if ( ( byteSize % wordSwap_ ) != 0 ) {
			throw InvalidArgError("wordSwap does not divide size");
		}
	}

	// merging a word-swapped entity with a bit-size that is
	// not a multiple of 8 would require more complex bitmasks
	// than what our current 'write' method supports.
	if ( (sizeBits_ % 8) && wordSwap_ && mode_ != IVal_Base::RO ) {
		throw InvalidArgError("Word-swap only supported if size % 8 == 0");
	}

	if ( IScalVal_Base::NONE != encoding_ ) {
		if (   IScalVal_Base::ASCII  == encoding_
			|| IScalVal_Base::EBCDIC == encoding_
			|| IScalVal_Base::UTF_8  == encoding_
			|| (IScalVal_Base::ISO_8859_1 <= encoding_ && IScalVal_Base::ISO_8859_16 >= encoding_) ) {
			if ( sizeBits_ != 8 )
				throw InvalidArgError("Character encoding expects 8-bit characters");
		} else if ( IScalVal_Base::UTF_16 == encoding_ ) {
			if ( sizeBits_ != 16 )
				throw InvalidArgError("UTF_16 encoding expects 16-bit characters");
		} else if ( IScalVal_Base::UTF_32 == encoding_ ) {
			if ( sizeBits_ != 32 )
				throw InvalidArgError("UTF_32 encoding expects 32-bit characters");
		} else if ( IScalVal_Base::IEEE_754 == encoding_ ) {
			if ( sizeBits_ != 32 && sizeBits_ != 64 )
				throw InvalidArgError("IEEE_754 encoding expects 32- or 64-bit numbers");
			if ( enum_ )
				throw InvalidArgError("IEEE_754 encoding doesn't support enums");
			isSigned_ = true;
		}
	}

	configBase_ = checkConfigBase( configBase_ );
	size_       = computeSize(wordSwap_, sizeBits_, lsBit_);
}

void
CIntEntryImpl::postHook( ConstShObj ob )
{
	CEntryImpl::postHook( ob );
	// maybe the parameters were modified by a subclass? check again
	checkArgs();
}

CIntEntryImpl::CIntEntryImpl(Key &k, const char *name, uint64_t sizeBits, bool isSigned, int lsBit, Mode mode, unsigned wordSwap, Enum enm)
: CEntryImpl(
		k,
		name,
		computeSize(wordSwap, sizeBits, lsBit)
	),
	isSigned_(isSigned),
	lsBit_(lsBit),
	sizeBits_(sizeBits),
	mode_(mode),
	wordSwap_( wordSwap ),
	encoding_( DFLT_ENCODING ),
	enum_(enm),
	configBase_( DFLT_CONFIG_BASE )
{
}

bool
CIntEntryImpl::isSigned() const
{
	return isSigned_;
}

int
CIntEntryImpl::getLsBit() const
{
	return lsBit_;
}
uint64_t
CIntEntryImpl::getSizeBits() const
{
	return sizeBits_;
}

unsigned
CIntEntryImpl::getWordSwap() const
{
	return wordSwap_;
}

CIntEntryImpl::Encoding
CIntEntryImpl::getEncoding() const
{
	return encoding_;
}

int
CIntEntryImpl::getConfigBase() const
{
	return configBase_;
}

IIntField::Mode
CIntEntryImpl::getMode() const
{
	return mode_;
}

Enum
CIntEntryImpl::getEnum() const
{
	return enum_;
}


// setters can only be used from constructors
void
CIntEntryImpl::setSigned(Key &k, bool     v)
{
	isSigned_   = v;
}

void
CIntEntryImpl::setLsBit(Key &k, int      v)
{
	lsBit_      = v;
}

void
CIntEntryImpl::setSizeBits(Key &k, uint64_t v)
{
	sizeBits_   = v;
}

void
CIntEntryImpl::setWordSwap(Key &k, unsigned v)
{
	wordSwap_   = v;
}

void
CIntEntryImpl::setMode(Key &k, Mode     v)
{
	mode_       = v;
}

void
CIntEntryImpl::setEnum(Key &k, Enum     v)
{
	enum_       = v;
}

int
CIntEntryImpl::getDefaultConfigPrio() const
{
	return IVal_Base::RW == mode_ ? DFLT_CONFIG_PRIO_RW : CONFIG_PRIO_OFF;
}

void
CIntEntryImpl::setConfigBase(int proposed)
{
	if ( getLocked() )
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	configBase_ = checkConfigBase( proposed );
}

void
CIntEntryImpl::setEncoding(Encoding proposed)
{
	if ( getLocked() )
		throw ConfigurationError("Configuration Error - cannot modify attached device");
	encoding_ = proposed;
}

CIntEntryImpl::CIntEntryImpl(Key &key, YamlState &node)
:CEntryImpl(key, node),
 isSigned_(DFLT_IS_SIGNED),
 lsBit_(DFLT_LS_BIT),
 sizeBits_(DFLT_SIZE_BITS),
 mode_(DFLT_MODE),
 wordSwap_(DFLT_WORD_SWAP),
 encoding_(DFLT_ENCODING),
 configBase_(DFLT_CONFIG_BASE)
{
MutableEnum e;

	readNode(node, YAML_KEY_isSigned,   &isSigned_  );
	readNode(node, YAML_KEY_lsBit,      &lsBit_     );
	readNode(node, YAML_KEY_sizeBits,   &sizeBits_  );
	readNode(node, YAML_KEY_mode,       &mode_      );
	readNode(node, YAML_KEY_wordSwap,   &wordSwap_  );
	readNode(node, YAML_KEY_encoding,   &encoding_  );
	readNode(node, YAML_KEY_configBase, &configBase_);

	YamlState enum_node( node.lookup( YAML_KEY_enums ) );

	if ( enum_node ) {
		enum_ = IMutableEnum::create( enum_node );
	}
}

void
CIntEntryImpl::dumpYamlPart(YAML::Node &node) const
{

	CEntryImpl::dumpYamlPart(node);

	if ( isSigned_ != DFLT_IS_SIGNED )
		writeNode(node, YAML_KEY_isSigned, isSigned_ );
	if ( lsBit_    != DFLT_LS_BIT    )
		writeNode(node, YAML_KEY_lsBit,    lsBit_    );
	if ( sizeBits_ != DFLT_SIZE_BITS )
		writeNode(node, YAML_KEY_sizeBits, sizeBits_ );
	if ( mode_      != DFLT_MODE      )
		writeNode(node, YAML_KEY_mode,     mode_     );
	if ( wordSwap_  != DFLT_WORD_SWAP )
		writeNode(node, YAML_KEY_wordSwap, wordSwap_ );
	if ( encoding_  != DFLT_ENCODING )
		writeNode(node, YAML_KEY_encoding, encoding_ );
	if ( configBase_ != DFLT_CONFIG_BASE )
		writeNode(node, YAML_KEY_configBase, configBase_ );

	if ( enum_ ) {
		YAML::Node enums;
		enum_->dumpYaml( enums );
		writeNode(node, YAML_KEY_enums, enums);
	}
}

EntryAdapt
CIntEntryImpl::createAdapter(IEntryAdapterKey &key, Path p, const std::type_info &interfaceType) const
{
	if ( isInterface<Val_Base>(interfaceType) || isInterface<ScalVal_Base>(interfaceType) ) {
		return _createAdapter< shared_ptr<IIntEntryAdapt> >(this, p);
	} else if ( isInterface<ScalVal>(interfaceType) ) {
		if ( getMode() != IVal_Base::RW ) {
			throw InterfaceNotImplementedError("ScalVal interface not supported (read- or write-only)");
		}
		if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
			throw InterfaceNotImplementedError("ScalVal interface not supported (IEEE_754 encoding)");
		}
		return _createAdapter<ScalValAdapt>(this, p);
	} else if ( isInterface<DoubleVal>(interfaceType) ) {
		if ( getMode() != IVal_Base::RW ) {
			throw InterfaceNotImplementedError("Double interface not supported (read- or write-only)");
		}
		return _createAdapter<DoubleValAdapt>(this, p);
	} else if ( isInterface<ScalVal_RO>(interfaceType) ) {
		if ( getMode() == IVal_Base::WO ) {
			throw InterfaceNotImplementedError("ScalVal_RO interface not supported (write-only)");
		}
		if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
			throw InterfaceNotImplementedError("ScalVal interface not supported (IEEE_754 encoding)");
		}
		return _createAdapter<ScalVal_ROAdapt>(this, p);
	} else if ( isInterface<DoubleVal_RO>(interfaceType) ) {
		if ( getMode() == IVal_Base::WO ) {
			throw InterfaceNotImplementedError("Double_RO interface not supported (write-only)");
		}
		return _createAdapter<DoubleVal_ROAdapt>(this, p);
	}
#if 0
	// without caching and bit-level access at the SRP protocol level we cannot
	// support write-only yet.
	else if ( isInterface<ScalVal_WO>(interfaceType) || isInterface<DoubleVal_WO>(interfaceType) ) {
		if ( getMode() == IVal_Base::RO ) {
			throw InterfaceNotImplementedError("ScalVal_WO/Double_WO interface not supported (read-only)");
		}
		return _createAdapter<ScalVal_WOAdapt>(this, p);
	}
#endif
	// maybe the superclass knows about this interface?
	return CEntryImpl::createAdapter(key, p, interfaceType);
}


IntField IIntField::create(const char *name, uint64_t sizeBits, bool isSigned, int lsBit, Mode mode, unsigned wordSwap)
{
	return CShObj::create<IntEntryImpl>(name, sizeBits, isSigned, lsBit, mode, wordSwap);
}

CScalValAdapt::CScalValAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie), CScalVal_ROAdapt(k, p, ie), CScalVal_WOAdapt(k, p, ie)
{
}

CDoubleValAdapt::CDoubleValAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie), CDoubleVal_ROAdapt(k, p, ie), CDoubleVal_WOAdapt(k, p, ie)
{
}


CScalVal_ROAdapt::CScalVal_ROAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
{
}

CDoubleVal_ROAdapt::CDoubleVal_ROAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
{
}

CScalVal_WOAdapt::CScalVal_WOAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
{
}

CDoubleVal_WOAdapt::CDoubleVal_WOAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
{
}


ScalVal_Base IScalVal_Base::create(Path p)
{
	return IEntryAdapt::check_interface<ScalVal_Base>( p );
}

DoubleVal_RO IDoubleVal_RO::create(Path p)
{
	return IEntryAdapt::check_interface<DoubleVal_RO>( p );
}

ScalVal_RO IScalVal_RO::create(Path p)
{
	return IEntryAdapt::check_interface<ScalVal_RO>( p );
}

ScalVal_WO IScalVal_WO::create(Path p)
{
	return IEntryAdapt::check_interface<ScalVal_WO>( p );
}

ScalVal IScalVal::create(Path p)
{
	return IEntryAdapt::check_interface<ScalVal>( p );
}

DoubleVal IDoubleVal::create(Path p)
{
	return IEntryAdapt::check_interface<DoubleVal>( p );
}

unsigned
IIntEntryAdapt::nelmsFromIdx(IndexRange *r)
{
	if ( r ) {
		int f = r->getFrom();
		int t = r->getTo();

		if ( f >= 0 || t >= 0 ) {
			CompositePathIterator it( p_ );
			if ( f < 0 )
				f = it->idxf_;
			else
				f = it->idxf_ + f;
			if ( t < 0 )
				t = it->idxt_;
			else
				t = it->idxf_ + t;
			t = t - f + 1;
			int d = it.getNelmsRight();
			if ( t >= 0 && t < d )
				d = t;
			return it.getNelmsLeft() * d;
		}
	}
	return getNelms();
}


class SignExtender {
private:
		bool     isSigned;
		int      signByte;
		uint8_t  signBit;
		uint8_t  signMsk;
		int      jinc;
		int      jend;
		unsigned dbytes;
public:
	SignExtender(bool isSigned, uint64_t sizeBits, unsigned dbytes)
		: isSigned(isSigned),
		  signByte( (sizeBits-1)/8 ),
		  signBit( 1<<( (sizeBits-1) & 7 ) ),
		  dbytes( dbytes )
	{
	bool littleEndian;
		signMsk      = signBit | (signBit - 1);
		littleEndian = LE == hostByteOrder();
		jinc         = (littleEndian ?     +1 : -1);
		jend         = (littleEndian ? dbytes : -1);
		if ( ! littleEndian ) {
			signByte = dbytes - 1 - signByte;
		}
	}

  
	void work(uint8_t *obufp, uint8_t *ibufp)
	{
		int j           = signByte;

#ifdef SVAL_DEBUG
printf("SE signByte %i, signBit %x\n", signByte, signBit);
#endif
		if ( isSigned && (ibufp[signByte] & signBit) != 0 ) {
			obufp[j] = ibufp[j] | ~signMsk;
			j += jinc;
			while ( j != jend ) {
				obufp[j] = 0xff;
				j += jinc;
			}
		} else {
			obufp[j] = ibufp[j] & signMsk;
			j += jinc;
			while ( j != jend ) {
				obufp[j] = 0x00;
				j += jinc;
			}
		}
		//for ( j=0; j <(int)dbytes; j++ ) printf("obuf[%i] 0x%02x ", j, obufp[j]);  printf("\n");
	}
};

class Swapper {
private:
	unsigned wlen;
	unsigned nwrds_2;
	unsigned jtop;

public:
	Swapper(unsigned nbytes, unsigned wlen)
	:wlen(wlen),
	 nwrds_2( wlen > 0 ? nbytes/wlen/2 : 0 ),
	 jtop(nbytes - wlen)
	{
	}

	void work(uint8_t *buf)
	{
	unsigned j, jinc;
		for ( j=jinc=0; j<nwrds_2; j++, jinc+=wlen  ) {
		uint8_t tmp[wlen];
				memcpy(tmp,               buf        + jinc, wlen);
				memcpy(buf        + jinc, buf + jtop - jinc, wlen);
				memcpy(buf + jtop - jinc,               tmp, wlen);
		}
	}
};

class BitShifter {
private:
	int  sbytes;
	int  shift;
	bool littleEndian;
public:
	BitShifter(int sbytes, int shift)
	:sbytes( sbytes ),
	 shift( shift ),
	 littleEndian( LE == hostByteOrder() )
	{
	}

	void shiftRight(uint8_t *bufp)
	{
		uint16_t tmp16;
		int      j;
		if ( littleEndian ) {
			tmp16 = 0;
			for ( j = sbytes - 1; j >= 0; j-- ) {
				tmp16 = (tmp16<<8) | bufp[j];
				bufp[j] = (tmp16 >> shift);
			}
		} else {
			tmp16 = 0;
			for (  j = 0; j < sbytes; j++ ) {
				tmp16 = (tmp16<<8) | bufp[j];
				bufp[j] = (tmp16 >> shift);
			}
		}
	}

	void shiftLeft(uint8_t *bufp)
	{
		uint16_t tmp16_1, tmp16_2;
		int      j;
		if ( ! littleEndian ) {
			tmp16_2 = 0;
			for ( j = sbytes - 1; j >= 0; j-- ) {
				tmp16_1 = bufp[j];
				tmp16_2 = (tmp16_2>>8) | (tmp16_1 << shift);
				bufp[j] = tmp16_2;
			}
		} else {
			tmp16_2 = 0;
			for (  j = 0; j < sbytes; j++ ) {
				tmp16_1 = bufp[j];
				tmp16_2 = (tmp16_2>>8) | (tmp16_1 << shift);
				bufp[j] = tmp16_2;
			}
		}
	}
};

void
CDoubleVal_ROAdapt::int2dbl(double *dst, uint64_t *src, unsigned nelms)
{
	if ( isSigned() ) {
		for ( unsigned i = 0; i<nelms; i++ )
			dst[i] = (int64_t)src[i];
	} else {
		for ( unsigned i = 0; i<nelms; i++ )
			dst[i] = src[i];
	}
}

void
CDoubleVal_ROAdapt::dbl2dbl(double *srcdst, unsigned nelms)
{
}

void
CDoubleVal_WOAdapt::dbl2dbl(double *srcdst, unsigned nelms)
{
}


void
CDoubleVal_WOAdapt::dbl2int(uint64_t *dst, double *src, unsigned nelms)
{
	if ( isSigned() ) {
		for ( unsigned i = 0; i<nelms; i++ )
			dst[i] = (int64_t)src[i];
	} else {
		for ( unsigned i = 0; i<nelms; i++ )
			dst[i] = (uint64_t)src[i];
	}
}

typedef shared_ptr<IIntEntryAdapt> IntEntryAdapt;

class CGetValContext : public CAsyncIOCompletion {
protected:
	static const unsigned ALLOC_BRK = 512;

	IntEntryAdapt   adapter_;

private:
	typedef union { uint8_t c; uint64_t u; } BufEl;
	BufEl           tmpBuf_[(ALLOC_BRK + sizeof(BufEl) - 1)/sizeof(BufEl)];
	uint8_t        *tmpBufAlloc_;
	unsigned        tmpBufSz_;

protected:

	CGetValContext(IntEntryAdapt adapter, AsyncIO aio = AsyncIO())
	: CAsyncIOCompletion( aio ),
	  adapter_ ( adapter ),
	  tmpBufSz_( 0       )
	{
	}

	uint8_t *mkbuf(unsigned buf_bytes)
	{
	uint8_t *rval;
		if ( buf_bytes <= ALLOC_BRK ) {
			// use internal buffer
			rval   = &tmpBuf_[0].c;
		} else if ( buf_bytes <= tmpBufSz_ ) {
			// use existing buffer
			rval   = tmpBufAlloc_;
		} else {
			if ( tmpBufSz_ )
				delete [] tmpBufAlloc_;
			tmpBufSz_    = buf_bytes;
			tmpBufAlloc_ = new uint8_t[tmpBufSz_];
			rval         = tmpBufAlloc_;
		}
		return rval;
	}

	virtual void doComplete() = 0;

	virtual void complete()
	{
		// reset the adapter shared pointer before
		// the user callback is executed by the transaction
		// manager; helps synchronizing with shutdown since
		// otherwise the transaction manager may still hold
		// a shared ref. and if the user thread is scheduled
		// first then the user doesn't hold the *last* ref.
		doComplete();
		adapter_.reset();
	}

public:
	virtual ~CGetValContext()
	{
		if ( tmpBufSz_ )
			delete [] tmpBufAlloc_;
	}

};

class CGetIntValContext : public CGetValContext {
private:
	unsigned        dbytes_;
	ByteOrder       targetEndian_;
	unsigned        nelms_;

	uint8_t        *obufp_;
	uint8_t        *ibufp_;

public:
	CGetIntValContext(IntEntryAdapt adapter, uint8_t *buf, unsigned nelms, unsigned dbytes, ByteOrder targetEndian, AsyncIO aio = AsyncIO())
	: CGetValContext( adapter, aio )
	{
	unsigned  sbytes     = adapter->getSize();
	ByteOrder hostEndian = hostByteOrder();

		dbytes_       = dbytes;
		targetEndian_ = NATIVE == targetEndian ? hostEndian : targetEndian;
		nelms_        = nelms;
		obufp_        = buf;

		if ( sbytes > dbytes_ ) {
			ibufp_ = mkbuf( sbytes * nelms_ );
		} else {
			ibufp_ = buf;
		}

		if ( dbytes_ > sbytes ) {
			// obuf and ibuf overlap;
			if ( BE == hostEndian ) {
				// work top down
				ibufp_ += (dbytes_-sbytes) * nelms_;
			}
		}
	}

	void getReadParms(CReadArgs *ra)
	{
		ra->dst_       = ibufp_;
		ra->nbytes_    = adapter_->getSize();
	}

	void doComplete()
	{
	int         lsb          = adapter_->getLsBit();
	unsigned    sizeBits     = adapter_->getSizeBits();
	bool        sign_extend  = sizeBits < 8*dbytes_;
	bool        truncate     = sizeBits > 8*dbytes_;
	unsigned    wlen         = adapter_->getWordSwap();
	ByteOrder   hostEndian   = hostByteOrder();
	// byte-size including lsb shift
	unsigned    sbytes       = adapter_->getSize();
	// byte-size w/o lsb shift
	unsigned    nbytes       = (sizeBits + 7)/8;


		if (   targetEndian_ != hostEndian
				|| lsb                != 0
				|| sign_extend
				|| truncate
				|| wlen               >  0 ) {

			// transformation necessary
			SignExtender signExtend( adapter_->isSigned(), sizeBits, dbytes_ );
			Swapper      byteSwap( sbytes,    1 );
			Swapper      wordSwap( nbytes, wlen );
			BitShifter   bits    ( sbytes, lsb  );

			int ioff = (BE == hostEndian ? sbytes - dbytes_ : 0 );
			int noff = (BE == hostEndian ? sbytes - nbytes  : 0 );
			int ooff;
			int iidx, oidx, n, iinc, oinc;
			if ( ioff < 0 ) {
				ooff = -ioff;
				ioff = 0;
			} else {
				ooff = 0;
			}

			if ( BE == hostEndian ) {
				// top down
				oidx = 0;
				iidx = 0;
				iinc = +sbytes;
				oinc = +dbytes_;
			} else {
				// bottom up
				oidx = (nelms_-1)*dbytes_;
				iidx = (nelms_-1)*sbytes;
				iinc = -sbytes;
				oinc = -dbytes_;
			}
			for ( n = nelms_-1; n >= 0; n--, oidx += oinc, iidx += iinc ) {

				if ( targetEndian_ != hostEndian ) {
					byteSwap.work( ibufp_ + iidx );
				}

				if ( lsb != 0 ) {
					bits.shiftRight( ibufp_ + iidx );
				}

				if ( wlen  > 0 ) {
					//printf("pre-wswap (nbytes %i, iidx %i): ", nbytes, iidx);
					//for(j=0;j<sbytes;j++)
					//	printf("%02x ", ibufp_[iidx+j]);
					//printf("\n");
					wordSwap.work(ibufp_ + iidx + noff);
					//printf("pst-wswap: ");
					//for(j=0;j<sbytes;j++)
					//	printf("%02x ", ibufp_[iidx+j]);
					//printf("\n");
				}

				//printf("TRUNC oidx %i, ooff %i,  iidx %i, ioff %i, dbytes %i, sbytes %i\n", oidx, ooff, iidx, ioff, dbytes, sbytes);
				//for ( int j=0; j <sbytes; j++ ) printf("ibuf[%i] 0x%02x ", j, ibufp_[iidx+ioff+j]);  printf("\n");
				memmove( obufp_ + oidx + ooff, ibufp_ + iidx + ioff, dbytes_ >= sbytes ? sbytes : dbytes_ );
				//for ( int j=0; j <sbytes; j++ ) printf("obuf[%i] 0x%02x ", j, obufp_[oidx+ooff+j]);  printf("\n");

				// sign-extend
				if ( sign_extend ) {
					signExtend.work(obufp_+oidx, obufp_+oidx);
				}
			}
		}
	}

};


unsigned IIntEntryAdapt::checkNelms(unsigned nelms, SlicedPathIterator *it)
{
unsigned nelmsOnPath  = (*it).getNelmsLeft();

	if ( nelms >= nelmsOnPath ) {
		nelms = nelmsOnPath;
	} else {
		throw InvalidArgError("Invalid Argument: buffer too small");
	}
	return nelms;
}

unsigned IIntEntryAdapt::getVal(AsyncIO aio, uint8_t *buf, unsigned nelms, unsigned elsz, SlicedPathIterator *it)
{
Address            cl           = (*it)->c_p_;
ByteOrder          targetEndian = cl->getByteOrder();

	nelms = checkNelms(nelms, it);

	CReadArgs args;

	shared_ptr<CGetIntValContext> ctxt = cpsw::make_shared<CGetIntValContext> (
	                                             getSelfAs<IntEntryAdapt>(),
	                                             buf,
                                                 nelms,
                                                 elsz,
                                                 targetEndian,
                                                 aio );
	args.cacheable_ = ie_->getCacheable();
	args.off_       = 0;
	args.aio_       = ctxt;

	ctxt->getReadParms( &args );

	cl->read( it, &args );

	return nelms;
}

unsigned IIntEntryAdapt::getVal(uint8_t *buf, unsigned nelms, unsigned elsz, SlicedPathIterator *it)
{
Address            cl           = (*it)->c_p_;
ByteOrder          targetEndian = cl->getByteOrder();

	nelms = checkNelms( nelms, it );

	CReadArgs args;

	CGetIntValContext ctxt( getSelfAs<IntEntryAdapt>(), buf, nelms, elsz, targetEndian );

	args.cacheable_ = ie_->getCacheable();
	args.off_       = 0;
	ctxt.getReadParms( &args );

	cl->read( it, &args );

	ctxt.callback( 0 );
	return nelms;

}

class CGetStringValContext : public CGetValContext {
private:
	uint8_t       *buf_;
	unsigned       nelms_;
	unsigned       elsz_;
	CString       *strs_;

public:
	CGetStringValContext(IntEntryAdapt handle, CString *strs, unsigned nelms, AsyncIO stack = AsyncIO())
	: CGetValContext( handle, stack ),
	  nelms_   ( nelms                           ),
	  elsz_    ( sizeof(uint64_t)                ),
	  strs_    ( strs                            )
	{
	unsigned sz = sizeof(*buf_) * nelms_;
	unsigned elsz;
		if ( ! adapter_->getEnum() && (elsz = (adapter_->getSizeBits() + 7) / 8) > sizeof(uint64_t) ) {
			elsz_ = elsz;
		}
		sz  *= elsz_;
		buf_ = mkbuf( sz );
	}

	uint8_t *getTmpBuf()
	{
		return buf_;
	}

	unsigned getElsz()
	{
		return elsz_;
	}

	void doComplete()
	{
	Enum     enm = adapter_->getEnum();
	unsigned i;

		if ( enm || elsz_ <= sizeof(uint64_t) ) {
			if ( enm ) {
				for ( i=0; i < nelms_; i++ ) {
					IEnum::Item item = enm->map( reinterpret_cast<uint64_t*>( buf_ )[i] );
					strs_[i] = item.first;
				}
			} else {
				char strbuf[100];
				const char *fmt;
				strbuf[sizeof(strbuf)-1] = 0;
/*
				if ( 16 == adapter_->getConfigBase() ) {
					fmt = "%" PRIx64;
				} else
*/
				{
					fmt = adapter_->isSigned() ? "%" PRIi64 : "%" PRIu64;
				}
				for ( i=0; i < nelms_; i++ ) {
					::snprintf(strbuf, sizeof(strbuf) - 1, fmt, reinterpret_cast<uint64_t*>( buf_ )[i]);
					strs_[i] = cpsw::make_shared<string>( strbuf );
				}
			}
		} else {
			uint8_t *bufp = buf_;
			for ( i=0; i < nelms_; i++ ) {
				char strbuf[10];
				std::string s("0x");
				int         j;
				if ( LE == hostByteOrder() ) {
					for ( j = elsz_ - 1; j >= 0; j-- ) {
						::snprintf(strbuf, sizeof(strbuf), "%02x", bufp[j]);
						s.append( strbuf );
					}
				} else {
					for ( j = 0; j < (int)elsz_; j++ ) {
						snprintf(strbuf, sizeof(strbuf), "%02x", bufp[j]);
						s.append( strbuf );
					}
				}
				strs_[i] = cpsw::make_shared<string>( s );

				bufp += elsz_;
			}
		}
		while ( i < nelms_ )
			strs_[i].reset();
	}
};

unsigned CScalVal_ROAdapt::getVal(AsyncIO aio, CString *strs, unsigned nelms, IndexRange *range)
{
SlicedPathIterator it(p_, range);
unsigned           rval;

	nelms = checkNelms(nelms, &it);

	shared_ptr<CGetStringValContext> ctxt = cpsw::make_shared<CGetStringValContext>( getSelfAs<IntEntryAdapt>(), strs, nelms, aio);

	uint8_t *tmpBuf = ctxt->getTmpBuf();

	rval = getVal( ctxt, tmpBuf, nelms, ctxt->getElsz(), &it );

	return rval;
}

unsigned CScalVal_ROAdapt::getVal(CString *strs, unsigned nelms, IndexRange *range)
{
SlicedPathIterator it(p_, range);
unsigned           rval;

	nelms = checkNelms(nelms, &it);

	CGetStringValContext ctxt( getSelfAs<IntEntryAdapt>(), strs, nelms );

	uint8_t *tmpBuf = ctxt.getTmpBuf();

	rval = getVal( tmpBuf, nelms, ctxt.getElsz(), &it );

	ctxt.callback( 0 );

	return rval;
}

class CGetDoubleValContext : public CGetValContext {
private:
	// we can't easily obtain a shared pointer for the  DoubleVal_ROAdapter
	// (CShObj is a virtual base). However, CGetValContext already
	// holds a handle to a base class so we are save with a normal pointer here...
	CDoubleVal_ROAdapt *adapter_;
	double             *buf_;
	unsigned            nelms_;

public:
	CGetDoubleValContext(IntEntryAdapt handle, CDoubleVal_ROAdapt *adapter, double *buf, unsigned nelms, AsyncIO stack = AsyncIO())
	: CGetValContext( handle, stack ),
	  adapter_ ( adapter ),
	  buf_     ( buf     ),
	  nelms_   ( nelms   )
	{
	}

	void doComplete()
	{
	int i;

		if ( IScalVal_Base::IEEE_754 == adapter_->getEncoding() ) {
			if ( 64 != adapter_->getSizeBits() ) {
				for ( i=(nelms_ - 1); i>=0; i-- ) {
					float f;
					::memcpy( &f, reinterpret_cast<float*>(buf_) + i, sizeof(f) );
					buf_[i] = (double)f;
				}
			}
		} else {
			// FIXME -- this is a violation of the alias rule -- we might
			//          have to allocate an intermediate buffer...
			adapter_->int2dbl( buf_, reinterpret_cast<uint64_t*>(buf_), nelms_ );
		}

		adapter_->dbl2dbl( buf_, nelms_ );
	}
};

unsigned CDoubleVal_ROAdapt::getVal(AsyncIO aio, double *buf, unsigned nelms, IndexRange *range)
{
SlicedPathIterator it(p_, range);

unsigned rval;

	nelms = checkNelms(nelms, &it);

shared_ptr<CGetDoubleValContext> ctxt = cpsw::make_shared<CGetDoubleValContext>( getSelfAs<IntEntryAdapt>(), this, buf, nelms, aio );

	if ( IScalVal_Base::IEEE_754 == getEncoding() && 64 != getSizeBits() ) {
		float *fBuf = reinterpret_cast<float*>(buf);
		rval = IIntEntryAdapt::getVal<float>( ctxt, fBuf, nelms, &it );
	} else {
		rval = IIntEntryAdapt::getVal<double>( ctxt, buf, nelms, &it );
	}

	return rval;
}

unsigned CDoubleVal_ROAdapt::getVal(double *buf, unsigned nelms, IndexRange *range)
{
SlicedPathIterator it(p_, range);

unsigned rval;

	nelms = checkNelms(nelms, &it);

	CGetDoubleValContext ctxt( getSelfAs<IntEntryAdapt>(), this, buf, nelms );

	if ( IScalVal_Base::IEEE_754 == getEncoding() && 64 != getSizeBits() ) {
		float *fBuf = reinterpret_cast<float*>(buf);
		rval = IIntEntryAdapt::getVal<float>( fBuf, nelms, &it );
	} else {
		rval = IIntEntryAdapt::getVal<double>( buf, nelms, &it );
	}

	ctxt.callback( 0 );

	return rval;
}

#ifdef SVAL_DEBUG
static void prib(const char *pre, uint8_t *b)
{
printf("%s - ", pre);
for (int i=0; i<9; i++ ) {
	printf("%02x ", b[i]);
}
	printf("\n");
}
#endif

unsigned IIntEntryAdapt::setVal(uint8_t *buf, unsigned nelms, unsigned elsz, IndexRange *range)
{
SlicedPathIterator   it( p_, range );
Address          cl = it->c_p_;
uint64_t         off = 0;
unsigned         dbytes   = getSize(); // byte-size including lsb shift
uint64_t         sizeBits = getSizeBits();
unsigned         nbytes   = (sizeBits + 7)/8;  // byte-size w/o lsb shift
unsigned         sbytes   = elsz;
int              lsb      = getLsBit();
ByteOrder        hostEndian= hostByteOrder();
ByteOrder        targetEndian = cl->getByteOrder();
uint8_t          msk1     = 0x00;
uint8_t          mskn     = 0x00;

	if ( NATIVE == targetEndian ) {
		targetEndian = hostEndian;
	}

	nelms = checkNelms( nelms, &it );

	bool sign_extend = sizeBits > 8*sbytes;
	bool truncate    = sizeBits < 8*sbytes;
	unsigned wlen    = getWordSwap();

	bool need_work =     targetEndian != hostEndian
		              || lsb                != 0
	                  || sign_extend
		              || truncate
		              || wlen               >  0;

	size_t  obuf_nchars = need_work ? dbytes * nelms : 0;

	TMP_BUF_DECL(uint8_t, obuf, obuf_nchars );

	uint8_t *ibufp = buf;
	uint8_t *obufp = need_work ? obuf.getBufp() : buf;


	if ( need_work ) {

		// transformation necessary
		SignExtender signExtend( isSigned(), 8*sbytes, dbytes );
		Swapper      byteSwap( dbytes,    1 );
		Swapper      wordSwap( nbytes, wlen );
		BitShifter   bits    ( dbytes, lsb  );

		int ioff = (BE == hostEndian ? sbytes - dbytes : 0 );
		int noff = (BE == hostEndian ? dbytes - nbytes : 0 );
		int ooff;
		int iidx, oidx, n, iinc, oinc;
		if ( ioff < 0 ) {
			ooff = -ioff;
			ioff = 0;
		} else {
			ooff = 0;
		}

		if ( BE == hostEndian ) {
			// top down
			oidx = 0;
			iidx = 0;
			iinc = +sbytes;
			oinc = +dbytes;
		} else {
			// bottom up
			oidx = (nelms-1)*dbytes;
			iidx = (nelms-1)*sbytes;
			iinc = -sbytes;
			oinc = -dbytes;
		}

		msk1 =   (1<<lsb) - 1;
		mskn = ~ ((1<< ( (lsb + sizeBits) % 8 )) - 1);
		if ( mskn == 0xff )
			mskn = 0x00;
		if ( BE == targetEndian ) {
			uint8_t tmp = msk1;
			msk1 = mskn;
			mskn = tmp;
		}

#ifdef SVAL_DEBUG
printf("For NELMS %i, iinc %i, ioff %i, oinc %i, ooff %i, noff %i\n", nelms, iinc, ioff, oinc, ooff, noff);
#endif
		for ( n = nelms-1; n >= 0; n--, oidx += oinc, iidx += iinc ) {

#ifdef SVAL_DEBUG
prib("orig", ibufp+iidx);
#endif
			memcpy( obufp + oidx + ooff, ibufp + iidx + ioff, dbytes >= sbytes ? sbytes : dbytes );
#ifdef SVAL_DEBUG
prib("after memcpy", obufp + oidx);
#endif

			if ( sign_extend ) {
				signExtend.work(obufp + oidx, obufp + oidx);
#ifdef SVAL_DEBUG
prib("sign-extended", obufp + oidx);
#endif
			}

			if ( wlen  > 0 ) {
				wordSwap.work( obufp + oidx + noff );
#ifdef SVAL_DEBUG
prib("word-swapped", obufp + oidx);
#endif
			}

			if ( lsb != 0 ) {
				bits.shiftLeft( obufp + oidx );
#ifdef SVAL_DEBUG
prib("shifted", obufp + oidx);
#endif
			}

			if ( targetEndian != hostEndian ) {
				byteSwap.work( obufp + oidx );
#ifdef SVAL_DEBUG
prib("byte-swapped", obufp + oidx);
#endif
			}
		}
	}

	CWriteArgs args;

	args.cacheable_ = ie_->getCacheable();
	args.src_       = obufp;
	args.off_       = off;
	args.nbytes_    = dbytes;
	args.msk1_      = msk1;
	args.mskn_      = mskn;

	cl->write( &it, &args );

	return nelms;
}

unsigned CScalVal_WOAdapt::setVal(const char* *strs, unsigned nelms, IndexRange *range)
{
TMP_BUF_DECL(uint64_t, buf, nelms );
unsigned         i;
Enum             enm = getEnum();

	if ( enm ) {
		for ( i=0; i<nelms; i++ ) {
			IEnum::Item item = enm->map( strs[i] );
			buf[i] = item.second;
		}
	} else {
		const char *fmt = isSigned() ? "%" SCNi64 : "%" SCNu64;
		for ( i=0; i<nelms; i++ ) {
			if ( 1 != sscanf(strs[i], fmt, &buf[i]) ) {
				throw ConversionError("CScalVal_RO::setVal -- unable to scan string into number");
			}
		}
	}

	return setVal(buf.getBufp(), nelms, range);
}


unsigned CScalVal_WOAdapt::setVal(const char *v, IndexRange *range)
{
unsigned nelms = nelmsFromIdx( range );
VALS_BUF_DECL( const char*, const char *, vals, nelms);
	return vals.setVal(this, range, v);
}

unsigned CDoubleVal_WOAdapt::setVal(double *buf, unsigned nelms, IndexRange *range)
{
unsigned rval;

	dbl2dbl( buf, nelms );

	if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
		if ( 64 == getSizeBits() ) {
			rval = IIntEntryAdapt::setVal<double>( buf, nelms, range );
		} else {
			TMP_BUF_DECL(float, tmpBuf, nelms );
			for ( unsigned i=0; i<nelms; i++ ) {
				tmpBuf[i] = (float)buf[i];
			}
			rval = IIntEntryAdapt::setVal<float>( tmpBuf.getBufp(), nelms, range );
		}
	} else {
		TMP_BUF_DECL(uint64_t, tmpBuf, nelms );
		dbl2int( tmpBuf.getBufp(), buf, nelms );
		rval = IIntEntryAdapt::setVal<uint64_t>( tmpBuf.getBufp(), nelms, range );
	}

	return rval;
}

unsigned CDoubleVal_WOAdapt::setVal(double v, IndexRange *range)
{
TMP_BUF_DECL(uint64_t, buf, nelmsFromIdx( range ) );
uint64_t         lu;

	dbl2dbl( &v, 1 );

	if ( IScalVal_Base::IEEE_754 == getEncoding() ) {
		if ( 32 == getSizeBits() ) {
			float    f = (float)v;
			uint32_t u;
			memcpy( &u, &f, sizeof(u) );

			lu = (uint64_t)u;
		} else {
			memcpy( &lu, &v, sizeof(lu) );
		}
	} else {
		dbl2int( &lu, &v, 1 );
	}
	for ( unsigned i = 0; i < buf.getNelms(); i++ ) {
		buf[i] = lu;
	}
	return IIntEntryAdapt::setVal<uint64_t>( buf.getBufp(), buf.getNelms(), range );
}

unsigned CScalVal_WOAdapt::setVal(uint64_t  v, IndexRange *r)
{
unsigned nelms = nelmsFromIdx(r);

	// since writes may be collapsed at a lower layer we simply build an array here
	if ( getSize() <= sizeof(uint8_t) ) {
		VALS_BUF_DECL( uint8_t, uint64_t, vals, nelms );
		return vals.setVal( this, r, v );
	} else if ( getSize() <= sizeof(uint16_t) ) {
		VALS_BUF_DECL( uint16_t, uint64_t, vals, nelms );
		return vals.setVal( this, r, v );
	} else if ( getSize() <= sizeof(uint32_t) ) {
		VALS_BUF_DECL( uint32_t, uint64_t, vals, nelms );
		return vals.setVal( this, r, v );
	} else {
		VALS_BUF_DECL( uint64_t, uint64_t, vals, nelms );
		return vals.setVal( this, r, v );
	}
}

typedef union VU_ {
	double   d;
	uint64_t u;
} VU;

uint64_t
CIntEntryImpl::dumpMyConfigToYaml(Path p, YAML::Node &node) const
{
	if ( IVal_Base::WO != getMode() ) {
		ScalVal_Base bas( IScalVal_Base::create( p ) );
		unsigned     nelms = bas->getNelms();

		if ( nelms == 0 ) {
			node = YAML::Node( YAML::NodeType::Undefined );
			return 0;
		}

		TMP_BUF_DECL(VU,   valBuf, nelms );

		unsigned     got;
		unsigned     i;

		bool         isFloat;

		if ( (isFloat = (IScalVal_Base::IEEE_754 == getEncoding())) ) {
			DoubleVal_RO val( IDoubleVal_RO::create( p ) );
			got = val->getVal( & valBuf[0].d, nelms, 0 );
		} else {
			ScalVal_RO val( IScalVal_RO::create( p ) );
			got = val->getVal( & valBuf[0].u, nelms, 0 );
		}

		if ( nelms != got ) {
			throw ConfigurationError("CIntEntryImpl::dumpMyConfigToYaml -- unexpected number of elements read");
		}

		// check if all values are identical - just do comparison of the bit pattern
		// (since Nan == Nan is false we would end up writing unnecessary stuff)
		uint64_t u0 = valBuf[0].u;
		for ( i=nelms-1; i>0; i-- ) {
			if ( u0 != valBuf[i].u )
				break;
		}

		// base 10 settings
		int field_width = 0;
		const char *fmt = isSigned() ? "%*" PRId64 : "%*" PRIu64;

		if ( 16 == getConfigBase() ) {
			// base 16 settings
			field_width = (getSizeBits() + 3) / 4; // one hex char per nibble
			fmt         = "0x%0*" PRIx64;
		}

		if ( i ) {
			// must save full array;
			YAML::Node n( YAML::NodeType::Sequence );

			if ( isFloat ) {
				for ( i=0; i<nelms; i++ ) {
					n.push_back( valBuf[i].d );
				}
			} else if ( enum_ ) {
				for ( i=0; i<nelms; i++ ) {
					n.push_back( *(enum_->map( valBuf[i].u ).first) );
				}
			} else {
				char cbuf[66];

				for ( i=0; i<nelms; i++ ) {
					// yaml-cpp dumps integers in decimal representation
					::snprintf(cbuf, sizeof(cbuf), fmt, field_width, valBuf[i].u);
					n.push_back( cbuf );
				}
			}
			node = n;
		} else {
			// can save single value
			YAML::Node n( YAML::NodeType::Scalar );

			if ( isFloat ) {
				n = valBuf[0].d;
			} else if ( enum_ ) {
				n = *enum_->map( valBuf[0].u ).first;
			} else {
				char cbuf[66];
				::snprintf(cbuf, sizeof(cbuf), fmt, field_width, valBuf[0].u);
				n = cbuf;
			}
			node = n;
		}
		return nelms;
	}
	node = YAML::Node( YAML::NodeType::Undefined );
	return 0;
}

uint64_t
CIntEntryImpl::loadMyConfigFromYaml(Path p, YAML::Node &n) const
{
unsigned nelms, i;

	if ( IVal_Base::RO == getMode() ) {
		throw ConfigurationError("Cannot load configuration into read-only ScalVal");
	}

	if ( !n )
		throw InvalidArgError("CIntEntryImpl::loadMyConfigFromYaml -- empty YAML node");

	if ( n.IsScalar() ) {
		nelms = 1;
	} else if ( ! n.IsSequence() ) {
		throw InvalidArgError("CIntEntryImpl::loadMyConfigFromYaml -- YAML node a Scalar nor a Sequence");
	} else {
		nelms = n.size();
	}

	if ( 0 == nelms )
		return 0;

	unsigned nelmsFromPath = p->getNelms();

	// A scalar means we will write all elements to same value
	if ( nelms < nelmsFromPath  && !n.IsScalar() ) {
		std::string str("CIntEntryImpl::loadMyConfigFromYaml --  elements in YAML node(");
                    str += p->toString();
                    str += ") < number expected from PATH";
		throw InvalidArgError(str);
	}

	if ( nelms > nelmsFromPath ) {
		fprintf(stderr,"WARNING: loadMyConfigFromYaml -- excess elements in YAML Node (%s); IGNORED\n", p->toString().c_str());
		nelms = nelmsFromPath;
	}

	bool isFloat = (IScalVal_Base::IEEE_754 == getEncoding());

	if ( n.IsScalar() ) {
		double   d;
		uint64_t u;
		if ( isFloat ) {
			d   = n.as<double>();
		} else if ( enum_ ) {
			const std::string &nam = n.as<std::string>();
			try {
				u = enum_->map( nam.c_str() ).second;
			} catch (ConversionError &e) {
				// try to fall-back on numerical
				try {
					u = n.as<uint64_t>();
				} catch (YAML::TypedBadConversion<unsigned long>) {
					throw e; // throw original error
				}
			}
		} else {
			u = n.as<uint64_t>();
		}
		if ( isFloat ) {
			DoubleVal val = IDoubleVal::create( p );
			val->setVal( d );
		} else {
			ScalVal val = IScalVal::create( p );
			val->setVal( u );
		}
		nelms = nelmsFromPath;
	} else {
		TMP_BUF_DECL(VU, valBuf, nelms );
		if ( isFloat ) {
			for ( i=0; i<nelms; i++ ) {
				valBuf[i].d = YAML::NodeFind(n, i).as<double>();
			}
		} else if ( enum_ ) {
			for ( i=0; i<nelms; i++ ) {
				const std::string &nam = YAML::NodeFind(n,i).as<std::string>();
				try {
					valBuf[i].u = enum_->map( nam.c_str() ).second;
				} catch (ConversionError &e) {
					// try to fall-back on numerical
					try {
						valBuf[i].u = YAML::NodeFind(n,i).as<uint64_t>();
					} catch (YAML::TypedBadConversion<unsigned long>) {
						throw e; // throw original error
					}
				}
			}
		} else {
			for ( i=0; i<nelms; i++ ) {
				valBuf[i].u = YAML::NodeFind(n,i).as<uint64_t>();
			}
		}
		if ( isFloat ) {
			DoubleVal val = IDoubleVal::create( p );
			val->setVal( &valBuf[0].d, nelms );
		} else {
			ScalVal val = IScalVal::create( p );
			val->setVal( &valBuf[0].u, nelms );
		}
	}
	return nelms;
}


CIntEntryImpl::CBuilder::CBuilder(Key &k)
:CShObj(k)
{
	init();
}

IIntField::Builder CIntEntryImpl::CBuilder::clone()
{
	return CShObj::clone( getSelfAs<BuilderImpl>() );
}

IIntField::Builder CIntEntryImpl::CBuilder::name(const char *name)
{
	name_ = name ? std::string(name) : std::string();
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::sizeBits(uint64_t sizeBits)
{
	sizeBits_ = sizeBits;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::isSigned(bool isSigned)
{
	isSigned_ = isSigned;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::lsBit(int lsBit)
{
	lsBit_    = lsBit;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::mode(Mode mode)
{
	mode_     = mode;
	if ( IVal_Base::RW != mode )
		configPrio( CONFIG_PRIO_OFF );
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::configPrio(int configPrio)
{
	configPrio_ = configPrio;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::configBase(int configBase)
{
	configBase_ = checkConfigBase( configBase );
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::wordSwap(unsigned wordSwap)
{
	wordSwap_ = wordSwap;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::encoding(Encoding encoding)
{
	encoding_ = encoding;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::setEnum(Enum enm)
{
	enum_ = enm;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::reset()
{
	init();
	return getSelfAs<BuilderImpl>();
}

void CIntEntryImpl::CBuilder::init()
{
	name_       = std::string();
	sizeBits_   = DFLT_SIZE_BITS;
	lsBit_      = DFLT_LS_BIT;
	isSigned_   = DFLT_IS_SIGNED;
	mode_       = DFLT_MODE;
	configPrio_ = IVal_Base::RW == DFLT_MODE ? DFLT_CONFIG_PRIO_RW : CONFIG_PRIO_OFF;
	configBase_ = DFLT_CONFIG_BASE;
	wordSwap_   = DFLT_WORD_SWAP;
	encoding_   = DFLT_ENCODING;
}

IntField CIntEntryImpl::CBuilder::build()
{
	return build( name_.c_str() );
}

IntField CIntEntryImpl::CBuilder::build(const char *name)
{
	// merging a word-swapped entity with a bit-size that is
	// not a multiple of 8 would require more complex bitmasks
	// than what our current 'write' method supports.
	if ( (sizeBits_ % 8) && wordSwap_ && mode_ != IVal_Base::RO ) {
		throw InvalidArgError("Word-swap only supported if size % 8 == 0 or mode == RO");
	}

	IntEntryImpl rval = CShObj::create<IntEntryImpl>(name, sizeBits_, isSigned_, lsBit_, mode_, wordSwap_, enum_);
	rval->setEncoding  ( encoding_   );
	rval->setConfigPrio( configPrio_ );
	rval->setConfigBase( configBase_ );
	return rval;
}

IIntField::Builder  CIntEntryImpl::CBuilder::create()
{
	return CShObj::create< shared_ptr<CIntEntryImpl::CBuilder> >();
}

IIntField::Builder IIntField::IBuilder::create()
{
	return CIntEntryImpl::CBuilder::create();
}

IndexRange::IndexRange(int from, int to)
:std::vector< std::pair<int,int> >(1)
{
	if ( from >= 0 && to >= 0 && to < from )
		throw InvalidArgError("Invalid index range");
	setFrom( from );
	setTo( to );
	(*this)[0].first = from;
	(*this)[0].first = from;
}

IndexRange::IndexRange(int fromto)
:std::vector< std::pair<int,int> >(1)
{
	setFromTo( fromto );
}

int IndexRange::getFrom() const
{
	return (*this)[0].first;
}

int IndexRange::getTo() const
{
	return (*this)[0].second;
}

void IndexRange::setFrom(int f)
{
	(*this)[0].first = f;
}

void IndexRange::setTo(int t)
{
	(*this)[0].second = t;
}

void IndexRange::setFromTo(int i)
{
	setFrom( i );
	setTo( i );
}

void IndexRange::setFromTo(int f, int t)
{
	setFrom( f );
	setTo( t );
}

IndexRange & IndexRange::operator++()
{
	int newFrom = getTo() + 1;
	setTo( newFrom + getTo() - getFrom() );
	setFrom( newFrom );
	return *this;
}
