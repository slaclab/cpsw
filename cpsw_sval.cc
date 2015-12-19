
#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <cpsw_sval.h>
#include <cpsw_address.h>

using boost::dynamic_pointer_cast;

IEntryAdapt::IEntryAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
	:CShObj(k),
	 ie_(ie), p_(p->clone())
{
	if ( p->empty() )
		throw InvalidPathError("<EMPTY>");

	Address  a = CompositePathIterator( &p )->c_p_;

	if ( a->getEntryImpl() != ie )
		throw InternalError("inconsistent args passed to IEntryAdapt");
	if ( UNKNOWN == a->getByteOrder() ) {
		throw ConfigurationError("Configuration Error: byte-order not set");
	}
}

template <typename ADAPT, typename IMPL> static ADAPT check_interface(Path p)
{
	if ( p->empty() )
		throw InvalidArgError("Empty Path");

	Address a = CompositePathIterator( &p )->c_p_;
	shared_ptr<const typename IMPL::element_type> e = dynamic_pointer_cast<const typename IMPL::element_type, CEntryImpl>( a->getEntryImpl() );
	if ( e ) {
		ADAPT rval = CShObj::template create<ADAPT>(p, e);
		return rval;
	}
	throw InterfaceNotImplementedError( p );
}

static uint64_t b2B(uint64_t bits)
{
	return (bits + 7)/8;
}

CIntEntryImpl::CIntEntryImpl(Key &k, const char *name, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
: CEntryImpl(
		k,
		name,
		wordSwap > 0 && wordSwap != b2B(sizeBits) ? b2B(sizeBits) + (lsBit ? 1 : 0) : b2B(sizeBits + lsBit)
	),
	is_signed_(is_signed),
	ls_bit_(lsBit),
	size_bits_(sizeBits),
	mode_(mode),
	wordSwap_(wordSwap)
{
unsigned byteSize = b2B(sizeBits);

	if ( wordSwap == byteSize )
		wordSwap = this->wordSwap_ = 0;

	if ( wordSwap > 0 ) {
		if ( ( byteSize % wordSwap ) != 0 ) {
			throw InvalidArgError("wordSwap does not divide size");
		}
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
ScalVal_ROAdapt rval = check_interface<ScalVal_ROAdapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( ! (rval->getMode() & IIntField::RO) ) 
			throw InterfaceNotImplementedError( p );
	}
	return rval;
}

class CStreamAdapt;
typedef shared_ptr<CStreamAdapt> StreamAdapt;

class CStreamAdapt : public IEntryAdapt, public virtual IStream {
public:
	CStreamAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie)
	: IEntryAdapt(k, p, ie)
	{
	}

	virtual int64_t read(uint8_t *buf, size_t size, CTimeout timeout, uint64_t off)
	{
		CompositePathIterator it( &p_);
		Address cl = it->c_p_;
		CReadArgs args;

		args.cacheable_ = ie_->getCacheable();
		args.dst_       = buf;
		args.dbytes_    = size;
		args.off_       = off;
		args.sbytes_    = size;
		args.timeout_   = timeout;
		return cl->read( &it, &args );
	}

};

Stream IStream::create(Path p)
{
StreamAdapt rval = check_interface<StreamAdapt, EntryImpl>( p );
	return rval;
}

#if 0
// without caching and bit-level access at the SRP protocol level we cannot
// support write-only yet.
ScalVal_WO IScalVal_WO::create(Path p)
{
ScalVal_WOAdapt rval = check_interface<ScalVal_WOAdapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( ! (rval->getMode() & IIntField::WO) ) 
			throw InterfaceNotImplementedError( p );
	}
	return rval;
}
#endif

ScalVal IScalVal::create(Path p)
{
ScalVal_Adapt rval = check_interface<ScalVal_Adapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( rval->getMode() != IIntField::RW )
			throw InterfaceNotImplementedError( p );
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
			if ( range->getFrom() >= 0 )
				f = range->getFrom();
			if ( range->getTo() >= 0 )
				t = range->getTo();
			if ( f < (*this)->idxf_ || t > (*this)->idxt_ || t < f )
				throw InvalidArgError("Array indices out of range");
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

	if ( nelms >= nelmsOnPath )
		nelms = nelmsOnPath;
	else
		throw InvalidArgError("Invalid Argument: buffer too small");

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
	args.dbytes_    = sbytes;
	args.off_       = off;
	args.sbytes_    = sbytes;
	
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
	args.sbytes_    = dbytes;
	args.off_       = off;
	args.dbytes_    = dbytes;
	args.msk1_      = msk1;
	args.mskn_      = mskn;
	
	cl->write( &it, &args );

	return nelms;
}
	
unsigned CScalVal_WOAdapt::setVal(uint64_t  v, IndexRange *r)
{
unsigned nelms;


	if ( r ) {
		int f = r->getFrom();
		int t = r->getTo();

		if ( f < 0 && t < 0 ) {
			nelms = getNelms();
		} else {
			CompositePathIterator it( &p_ );
			if ( f < 0 )
				f = it->idxf_; 
			if ( t < 0 )
				t = it->idxt_; 
			t = t - f + 1;
			int d = it.getNelmsRight();
			if ( t >= 0 && t < d )
				d = t;
			nelms = it.getNelmsLeft() * d;
		}
	} else {
		nelms = getNelms();
	}

	// since reads may be collapsed at a lower layer we simply build an array here
	if ( getSize() <= sizeof(uint8_t) ) {
		uint8_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms, r);
	} else if ( getSize() <= sizeof(uint16_t) ) {
		uint16_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms, r);
	} else if ( getSize() <= sizeof(uint32_t) ) {
		uint32_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms, r);
	} else {
		uint64_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms, r);
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
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::wordSwap(unsigned wordSwap)
{
	wordSwap_ = wordSwap;
	return getSelfAs<BuilderImpl>();
}

IIntField::Builder CIntEntryImpl::CBuilder::reset()
{
	init();
	return getSelfAs<BuilderImpl>();
}

void CIntEntryImpl::CBuilder::init()
{
	name_     = std::string();
	sizeBits_ = DFLT_SIZE_BITS;
	lsBit_    = DFLT_LS_BIT;
	isSigned_ = DFLT_IS_SIGNED;
	mode_     = DFLT_MODE;
    wordSwap_ = DFLT_WORD_SWAP;
}

IntField CIntEntryImpl::CBuilder::build()
{
	return build( name_.c_str() );
}

IntField CIntEntryImpl::CBuilder::build(const char *name)
{
	return CShObj::create<IntEntryImpl>(name, sizeBits_, isSigned_, lsBit_, mode_, wordSwap_);
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
