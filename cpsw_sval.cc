#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_sval.h>
#include <cpsw_address.h>

#include <string>

#include <cpsw_yaml.h>

using std::string;

const int CIntEntryImpl::DFLT_CONFIG_PRIO_RW;

class CStreamAdapt;
typedef shared_ptr<CStreamAdapt> StreamAdapt;

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
unsigned byteSize = b2B(size_bits_);

	if ( ls_bit_ > 7 ) {
		throw InvalidArgError("lsBit (bit shift) must be in 0..7");
	}

	if ( wordSwap_ == byteSize )
		wordSwap_ = 0;

	if ( wordSwap_ > 0 ) {
		if ( ( byteSize % wordSwap_ ) != 0 ) {
			throw InvalidArgError("wordSwap does not divide size");
		}
	}

	/* Encoding is not currently used by CPSW itself; just a hint for others;
	 * thus, don't check.
	 */

	configBase_ = checkConfigBase( configBase_ );
}


CIntEntryImpl::CIntEntryImpl(Key &k, const char *name, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap, Enum enm)
: CEntryImpl(
		k,
		name,
		computeSize(wordSwap, sizeBits, lsBit)
	),
	is_signed_(is_signed),
	ls_bit_(lsBit),
	size_bits_(sizeBits),
	mode_(mode),
	wordSwap_( wordSwap ),
	encoding_( DFLT_ENCODING ),
	enum_(enm),
	configBase_( DFLT_CONFIG_BASE )
{
	checkArgs();
}

int
CIntEntryImpl::getDefaultConfigPrio() const
{
	return RW == mode_ ? DFLT_CONFIG_PRIO_RW : CONFIG_PRIO_OFF;
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
 is_signed_(DFLT_IS_SIGNED),
 ls_bit_(DFLT_LS_BIT),
 size_bits_(DFLT_SIZE_BITS),
 mode_(DFLT_MODE),
 wordSwap_(DFLT_WORD_SWAP),
 encoding_(DFLT_ENCODING),
 configBase_(DFLT_CONFIG_BASE)
{
MutableEnum e;

	readNode(node, "isSigned",   &is_signed_ );
	readNode(node, "lsBit",      &ls_bit_    );
	readNode(node, "sizeBits",   &size_bits_ );
	readNode(node, "mode",       &mode_      );
	readNode(node, "wordSwap",   &wordSwap_  );
	readNode(node, "encoding",   &encoding_  );
	readNode(node, "configBase", &configBase_);

	YamlState enum_node( node.lookup( "enums" ) );

	if ( enum_node ) {
		enum_ = IMutableEnum::create( enum_node );
	}

	size_     = computeSize(wordSwap_, size_bits_, ls_bit_);

	checkArgs();
}

void
CIntEntryImpl::dumpYamlPart(YAML::Node &node) const
{

	CEntryImpl::dumpYamlPart(node);

	if ( is_signed_ != DFLT_IS_SIGNED )
		writeNode(node, "isSigned", is_signed_);
	if ( ls_bit_    != DFLT_LS_BIT    )
		writeNode(node, "lsBit",    ls_bit_   );
	if ( size_bits_ != DFLT_SIZE_BITS )
		writeNode(node, "sizeBits", size_bits_);
	if ( mode_      != DFLT_MODE      )
		writeNode(node, "mode",     mode_     );
	if ( wordSwap_  != DFLT_WORD_SWAP )
		writeNode(node, "wordSwap", wordSwap_ );
	if ( encoding_  != DFLT_ENCODING )
		writeNode(node, "encoding", encoding_ );
	if ( configBase_ != DFLT_CONFIG_BASE )
		writeNode(node, "configBase", configBase_ );

	if ( enum_ ) {
		YAML::Node enums;
		enum_->dumpYaml( enums );
		writeNode(node, "enums", enums);
	}
}

IntField IIntField::create(const char *name, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
{
	return CShObj::create<IntEntryImpl>(name, sizeBits, is_signed, lsBit, mode, wordSwap);
}

CScalVal_Adapt::CScalVal_Adapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie), CScalVal_ROAdapt(k, p, ie), CScalVal_WOAdapt(k, p, ie)
{
}

CScalVal_WOAdapt::CScalVal_WOAdapt(Key &k, Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(k, p, ie)
{
	// merging a word-swapped entity with a bit-size that is
	// not a multiple of 8 would require more complex bitmasks
	// than what our current 'write' method supports.
	if ( (ie->getSizeBits() % 8) && ie->getWordSwap() )
		throw InvalidArgError("Word-swap only supported if size % 8 == 0");
}


ScalVal_RO IScalVal_RO::create(Path p)
{
ScalVal_ROAdapt rval = IEntryAdapt::check_interface<ScalVal_ROAdapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( ! (rval->getMode() & IIntField::RO) ) 
			throw InterfaceNotImplementedError( p->toString() );
	}
	return rval;
}

class CStreamAdapt : public IEntryAdapt, public virtual IStream {
public:
	CStreamAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
	: IEntryAdapt(k, p, ie)
	{
	}

	virtual int64_t read(uint8_t *buf, uint64_t size, const CTimeout timeout, uint64_t off)
	{
		CompositePathIterator it( &p_);
		Address cl = it->c_p_;
		CReadArgs args;

		args.cacheable_ = ie_->getCacheable();
		args.dst_       = buf;
		args.nbytes_    = size;
		args.off_       = off;
		args.timeout_   = timeout;
		return cl->read( &it, &args );
	}

	virtual int64_t write(uint8_t *buf, uint64_t size, const CTimeout timeout)
	{
		CompositePathIterator it( &p_);
		Address cl = it->c_p_;
		CWriteArgs args;

		args.cacheable_ = ie_->getCacheable();
		args.off_       = 0;
		args.src_       = buf;
		args.nbytes_    = size;
		args.msk1_      = 0;
		args.mskn_      = 0;
		args.timeout_   = timeout;
		return cl->write( &it, &args );
	}

};

Stream IStream::create(Path p)
{
StreamAdapt rval = IEntryAdapt::check_interface<StreamAdapt, EntryImpl>( p );
	return rval;
}

#if 0
// without caching and bit-level access at the SRP protocol level we cannot
// support write-only yet.
ScalVal_WO IScalVal_WO::create(Path p)
{
ScalVal_WOAdapt rval = IEntryAdapt::check_interface<ScalVal_WOAdapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( ! (rval->getMode() & IIntField::WO) ) 
			throw InterfaceNotImplementedError( p->toString() );
	}
	return rval;
}
#endif

ScalVal IScalVal::create(Path p)
{
ScalVal_Adapt rval = IEntryAdapt::check_interface<ScalVal_Adapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( rval->getMode() != IIntField::RW )
			throw InterfaceNotImplementedError( p->toString() );
	}
	return rval;
}


unsigned IIntEntryAdapt::getNelms()
{
	return p_->getNelms();
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

class SlicedPathIterator : public CompositePathIterator {
public:
	// 'suffix' (if used) must live for as long as this object is used
	Path suffix;
	
	SlicedPathIterator(Path p, IndexRange *range)
	:CompositePathIterator( &p )
	{
		if ( range && range->size() > 0 ) {
			int f = (*this)->idxf_;
			int t = (*this)->idxt_;
			if ( range->size() != 1 ) {
				throw InvalidArgError("Currently only 1-level of indices supported, sorry");
			}
			if ( range->getTo() >= 0 )
				t = f + range->getTo();
			if ( range->getFrom() >= 0 )
				f += range->getFrom();
			if ( f < (*this)->idxf_ || t > (*this)->idxt_ || t < f ) {
				throw InvalidArgError("Array indices out of range");
			}
			if ( f != (*this)->idxf_ || t != (*this)->idxt_ ) {
				suffix = IPath::create();
				Address cl = (*this)->c_p_;
				++(*this);
				IPathImpl::toPathImpl( suffix )->append( cl, f, t );
				(*this).append(suffix);
			}
		}
	}
};

unsigned CScalVal_ROAdapt::getVal(uint8_t *buf, unsigned nelms, unsigned elsz, IndexRange *range)
{
SlicedPathIterator it( p_, range );

Address          cl           = it->c_p_;
uint64_t         off          = 0;
unsigned         sbytes       = getSize(); // byte-size including lsb shift
unsigned         nbytes       = (getSizeBits() + 7)/8;  // byte-size w/o lsb shift
unsigned         dbytes       = elsz;
int              lsb          = getLsBit();
ByteOrder        hostEndian   = hostByteOrder();
ByteOrder        targetEndian = cl->getByteOrder();
unsigned         ibuf_nchars;
unsigned         nelmsOnPath  = it->nelmsLeft_;

	if ( nelms >= nelmsOnPath ) {
		nelms = nelmsOnPath;
	} else {
		throw InvalidArgError("Invalid Argument: buffer too small");
	}

	if ( sbytes > dbytes )
		ibuf_nchars = sbytes * nelms;
	else
		ibuf_nchars = 0;

	uint8_t ibuf[ibuf_nchars];

	uint8_t *ibufp = ibuf_nchars ? ibuf : buf;
	uint8_t *obufp = buf;

	if ( dbytes > sbytes ) {
		// obuf and ibuf overlap;
		if ( BE == hostEndian ) {
			// work top down
			ibufp += (dbytes-sbytes) * nelms;
		}
	}

	CReadArgs args;

	args.cacheable_ = ie_->getCacheable();
	args.dst_       = ibufp;
	args.nbytes_    = sbytes;
	args.off_       = off;
	
	cl->read( &it, &args );

	bool sign_extend = getSizeBits() < 8*dbytes;
	bool truncate    = getSizeBits() > 8*dbytes;

	unsigned wlen    = getWordSwap();

	if (   targetEndian != hostEndian
		|| lsb                != 0
	    || sign_extend
		|| truncate
		|| wlen               >  0 ) {

		// transformation necessary
		SignExtender signExtend( isSigned(), getSizeBits(), dbytes );
		Swapper      byteSwap( sbytes,    1 );
		Swapper      wordSwap( nbytes, wlen );
		BitShifter   bits    ( sbytes, lsb  );

		int ioff = (BE == hostEndian ? sbytes - dbytes : 0 );
		int noff = (BE == hostEndian ? sbytes - nbytes : 0 );
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
		for ( n = nelms-1; n >= 0; n--, oidx += oinc, iidx += iinc ) {

			if ( targetEndian != hostEndian ) {
				byteSwap.work(ibufp + iidx);
			}

			if ( lsb != 0 ) {
				bits.shiftRight( ibufp + iidx );
			}

			if ( wlen  > 0 ) {
//printf("pre-wswap (nbytes %i, iidx %i): ", nbytes, iidx);
//for(j=0;j<sbytes;j++)
//	printf("%02x ", ibufp[iidx+j]);
//printf("\n");
				wordSwap.work(ibufp + iidx + noff);
//printf("pst-wswap: ");
//for(j=0;j<sbytes;j++)
//	printf("%02x ", ibufp[iidx+j]);
//printf("\n");
			}

//printf("TRUNC oidx %i, ooff %i,  iidx %i, ioff %i, dbytes %i, sbytes %i\n", oidx, ooff, iidx, ioff, dbytes, sbytes);
//for ( int j=0; j <sbytes; j++ ) printf("ibuf[%i] 0x%02x ", j, ibufp[iidx+ioff+j]);  printf("\n");
			memmove( obufp + oidx + ooff, ibufp + iidx + ioff, dbytes >= sbytes ? sbytes : dbytes );
//for ( int j=0; j <sbytes; j++ ) printf("obuf[%i] 0x%02x ", j, obufp[oidx+ooff+j]);  printf("\n");

			// sign-extend
			if ( sign_extend ) {
				signExtend.work(obufp+oidx, obufp+oidx);
			}
		}
	}
	return nelms;
}

unsigned CScalVal_ROAdapt::getVal(CString *strs, unsigned nelms, IndexRange *range)
{
uint64_t buf[nelms];
unsigned got,i;
Enum     enm = getEnum();

	got = getVal(buf, nelms, range);

	if ( enm ) {
		for ( i=0; i < got; i++ ) {
			IEnum::Item item = enm->map(buf[i]);
			strs[i] = item.first;
		}
	} else {
		char strbuf[100];
		const char *fmt = isSigned() ? "%" PRIi64 : "%" PRIu64;
		strbuf[sizeof(strbuf)-1] = 0;
		for ( i=0; i < got; i++ ) {
			snprintf(strbuf, sizeof(strbuf) - 1, fmt, buf[i]);
			strs[i] = make_shared<string>( strbuf );
		}
	}
	while ( i < nelms )
		strs[i].reset();

	return got;
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

unsigned CScalVal_WOAdapt::setVal(uint8_t *buf, unsigned nelms, unsigned elsz, IndexRange *range)
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
unsigned         nelmsOnPath = it->nelmsLeft_;

	if ( nelms >= nelmsOnPath )
		nelms = nelmsOnPath;
	else
		throw InvalidArgError("not enough values supplied");

	bool sign_extend = sizeBits > 8*sbytes;
	bool truncate    = sizeBits < 8*sbytes;
	unsigned wlen    = getWordSwap();

	bool need_work =     targetEndian != hostEndian
		              || lsb                != 0
	                  || sign_extend
		              || truncate
		              || wlen               >  0;

	size_t  obuf_nchars = need_work ? dbytes * nelms : 0;
	uint8_t obuf[obuf_nchars];

	uint8_t *ibufp = buf;
	uint8_t *obufp = need_work ? obuf : buf;


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
uint64_t buf[nelms];
unsigned i;
Enum     enm = getEnum();

	if ( enm ) {
		for ( i=0; i<nelms; i++ ) {
			IEnum::Item item = enm->map( strs[i] );
			buf[i] = item.second;
		}
	} else {
		const char *fmt = isSigned() ? "%"SCNi64 : "%"SCNu64;
		for ( i=0; i<nelms; i++ ) {
			if ( 1 != sscanf(strs[i],fmt,&buf[i]) ) {
				throw ConversionError("CScalVal_RO::setVal -- unable to scan string into number");
			}
		}
	}

	return setVal(buf, nelms, range);
}

static unsigned nelmsFromIdx(IndexRange *r, Path p, unsigned nelms)
{
	if ( r ) {
		int f = r->getFrom();
		int t = r->getTo();

		if ( f >= 0 || t >= 0 ) {
			CompositePathIterator it( &p );
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
			nelms = it.getNelmsLeft() * d;
		}
	}
	return nelms;
}

unsigned CScalVal_WOAdapt::setVal(const char *v, IndexRange *r)
{
unsigned nelms = nelmsFromIdx(r, p_, getNelms());
const char *arg[nelms];
unsigned i;
	for (i=0; i<nelms; i++ )
		arg[i] = v;

	return setVal(arg, nelms, r);
}

template <typename EL> class Vals {
private:
	EL          *vals_;
	unsigned     nelms_;

	static const unsigned STACK_BREAK = 1024*100;

public:
	Vals(unsigned nelms)
	: vals_( nelms*sizeof(EL) > STACK_BREAK ? new EL[nelms] : NULL ),
	  nelms_( nelms )
	{
	}

	unsigned setVal(CScalVal_WOAdapt *scalValAdapt, IndexRange *r, uint64_t v)
	{
	EL onStack[ vals_ ? 0 : nelms_ ];
	EL *valp = vals_ ? vals_ : onStack;

		for ( unsigned i = 0; i<nelms_; i++ )
			valp[i] = (EL)v;
		return scalValAdapt->setVal( valp, nelms_, r );
	}
	
	~Vals()
	{
		delete [] vals_;
	}
};
	
unsigned CScalVal_WOAdapt::setVal(uint64_t  v, IndexRange *r)
{
unsigned nelms = nelmsFromIdx(r, p_, getNelms());

	// since writes may be collapsed at a lower layer we simply build an array here
	if ( getSize() <= sizeof(uint8_t) ) {
		Vals<uint8_t> vals( nelms );
		return vals.setVal( this, r, v );
	} else if ( getSize() <= sizeof(uint16_t) ) {
		Vals<uint16_t> vals( nelms );
		return vals.setVal( this, r, v );
	} else if ( getSize() <= sizeof(uint32_t) ) {
		Vals<uint32_t> vals( nelms );
		return vals.setVal( this, r, v );
	} else {
		Vals<uint64_t> vals( nelms );
		return vals.setVal( this, r, v );
	}
}

YAML::Node
CIntEntryImpl::dumpMyConfigToYaml(Path p) const
{
	if ( WO != getMode() ) {
		ScalVal_RO val( IScalVal_RO::create( p ) );
		unsigned   nelms = val->getNelms();
		uint64_t   u64[ nelms ];
		unsigned   i;

		if ( nelms == 0 )
			return YAML::Node( YAML::NodeType::Undefined );

		if ( nelms != val->getVal( u64, nelms ) ) {
			throw ConfigurationError("CIntEntryImpl::dumpMyConfigToYaml -- unexpected number of elements read");
		}

		// check if all values are identical
		for ( i=nelms-1; i>0; i-- ) {
			if ( u64[0] != u64[i] )
				break;
		}

		// base 10 settings
		int field_width = 0;
		const char *fmt = isSigned() ? "%*"PRId64 : "%*"PRIu64;

		if ( 16 == getConfigBase() ) {
			// base 16 settings
			field_width = (getSizeBits() + 3) / 4; // one hex char per nibble
			fmt         = "0x%0*"PRIx64;
		}

		if ( i ) {
			// must save full array;
			YAML::Node n( YAML::NodeType::Sequence );

			if ( enum_ ) {
				for ( i=0; i<nelms; i++ ) {
					n.push_back( *(enum_->map( u64[i] ).first) );
				}
			} else {
				char cbuf[66];
				for ( i=0; i<nelms; i++ ) {
					// yaml-cpp dumps integers in decimal representation
					::snprintf(cbuf, sizeof(cbuf), fmt, field_width, u64[i]);
					n.push_back( cbuf );
				}
			}
			return n;
		} else {
			// can save single value
			YAML::Node n( YAML::NodeType::Scalar );
			if ( enum_ ) {
				n = *enum_->map( u64[0] ).first;
			} else {
				char cbuf[66];
				::snprintf(cbuf, sizeof(cbuf), fmt, field_width, u64[0]);
				n = cbuf;
			}
			return n;
		}
	}
	return YAML::Node( YAML::NodeType::Undefined );
}

void
CIntEntryImpl::loadMyConfigFromYaml(Path p, YAML::Node &n) const
{
unsigned nelms, i;

	if ( RO == getMode() ) {
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
		return;

	// A scalar means we will write all elements to same value 
	if ( nelms < p->getNelms()  && !n.IsScalar() ) {
		throw InvalidArgError("CIntEntryImpl::loadMyConfigFromYaml --  elements in YAML node < number expected from PATH");
	}

	if ( nelms > p->getNelms() ) {
		fprintf(stderr,"WARNING: loadMyConfnigFromYaml -- excess elements in YAML Node; IGNORED\n");
		nelms = p->getNelms();
	}

	uint64_t u64[nelms];

	ScalVal val( IScalVal::create(p) );

	if ( n.IsScalar() ) {
		if ( enum_ ) {
			u64[0] = enum_->map( n.as<std::string>().c_str() ).second;
		} else {
			u64[0] = n.as<uint64_t>();
		}
		val->setVal( u64[0] );
	} else {
		if ( enum_ ) {
			for ( i=0; i<nelms; i++ ) {
				u64[i] = enum_->map( n[i].as<std::string>().c_str() ).second;
			}
		} else {
			for ( i=0; i<nelms; i++ ) {
				u64[i] = n[i].as<uint64_t>();
			}
		}
		val->setVal( u64, nelms );
	}

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
	if ( RW != mode )
		configPrio( 0 );
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
	configPrio_ = RW == DFLT_MODE ? DFLT_CONFIG_PRIO_RW : 0;
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

int IndexRange::getFrom()
{
	return (*this)[0].first;
}

int IndexRange::getTo()
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
