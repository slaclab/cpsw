
#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry.h>
#include <cpsw_sval.h>
#include <cpsw_address.h>

using boost::dynamic_pointer_cast;

IEntryAdapt::IEntryAdapt(Path p, shared_ptr<const CEntryImpl> ie)
	:ie(ie), p(p->clone())
{
	if ( p->empty() )
		throw InvalidPathError("<EMPTY>");

	Address  a = CompositePathIterator( &p )->c_p;

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

	Address a = CompositePathIterator( &p )->c_p;
	shared_ptr<const typename IMPL::element_type> e = dynamic_pointer_cast<const typename IMPL::element_type, CEntryImpl>( a->getEntryImpl() );
	if ( e ) {
		return make_shared<typename ADAPT::element_type>(p, e);
	}
	throw InterfaceNotImplementedError( p );
}

static uint64_t b2B(uint64_t bits)
{
	return (bits + 7)/8;
}

CIntEntryImpl::CIntEntryImpl(FKey k, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
: CEntryImpl(
		k,
		wordSwap > 0 && wordSwap != b2B(sizeBits) ? b2B(sizeBits) + (lsBit ? 1 : 0) : b2B(sizeBits + lsBit)
	),
	is_signed(is_signed),
	ls_bit(lsBit), size_bits(sizeBits),
	mode(mode),
	wordSwap(wordSwap)
{
unsigned byteSize = b2B(sizeBits);

	if ( wordSwap == byteSize )
		wordSwap = this->wordSwap = 0;

	if ( wordSwap > 0 ) {
		if ( ( byteSize % wordSwap ) != 0 ) {
			throw InvalidArgError("wordSwap does not divide size");
		}
	}
}

IntField IIntField::create(const char *name, uint64_t sizeBits, bool is_signed, int lsBit, Mode mode, unsigned wordSwap)
{
	return CEntryImpl::create<CIntEntryImpl>(name, sizeBits, is_signed, lsBit, mode, wordSwap);
}

CScalVal_Adapt::CScalVal_Adapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie), CScalVal_ROAdapt(p, ie), CScalVal_WOAdapt(p, ie)
{
}

CScalVal_WOAdapt::CScalVal_WOAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie)
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

ScalVal_WO IScalVal_WO::create(Path p)
{
ScalVal_WOAdapt rval = check_interface<ScalVal_WOAdapt, IntEntryImpl>( p );
	if ( rval ) {
		if ( ! (rval->getMode() & IIntField::WO) ) 
			throw InterfaceNotImplementedError( p );
	}
	return rval;
}

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
	if ( nelms < 0 ) {
		CompositePathIterator it( & p );
		while ( ! it.atEnd() )
			++it;
		nelms = it.getNelmsRight();
	}
	return nelms;	
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


unsigned CScalVal_ROAdapt::getVal(uint8_t *buf, unsigned nelms, unsigned elsz)
{
CompositePathIterator it( & p );
Address          cl           = it->c_p;
uint64_t         off          = 0;
unsigned         sbytes       = getSize(); // byte-size including lsb shift
unsigned         nbytes       = (getSizeBits() + 7)/8;  // byte-size w/o lsb shift
unsigned         dbytes       = elsz;
int              lsb          = getLsBit();
ByteOrder        hostEndian   = hostByteOrder();
ByteOrder        targetEndian = cl->getByteOrder();
unsigned         ibuf_nchars;

	if ( (unsigned)getNelms() <= nelms )
		nelms = getNelms();
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
	
	cl->read( &it, ie->getCacheable(), ibufp, sbytes, off, sbytes );

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

unsigned CScalVal_WOAdapt::setVal(uint8_t *buf, unsigned nelms, unsigned elsz)
{
CompositePathIterator it( & p );
Address          cl = it->c_p;
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

	if ( (unsigned)getNelms() <= nelms )
		nelms = getNelms();
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

	cl->write( &it, ie->getCacheable(), obufp, dbytes, off, dbytes, msk1, mskn );

	return nelms;
}
	
unsigned CScalVal_WOAdapt::setVal(uint64_t  v)
{
unsigned nelms = getNelms();
	// since reads may be collapsed at a lower layer we simply build an array here
	if ( getSize() <= sizeof(uint8_t) ) {
		uint8_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms);
	} else if ( getSize() <= sizeof(uint16_t) ) {
		uint16_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms);
	} else if ( getSize() <= sizeof(uint32_t) ) {
		uint32_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms);
	} else {
		uint64_t vals[nelms];
		for ( unsigned i=0; i<nelms; i++ )
			vals[i] = v;
		return setVal(vals, nelms);
	}
}

CIntEntryImpl::CBuilder::CBuilder()
{
	init();
}

IIntField::Builder CIntEntryImpl::CBuilder::name(const char *name)
{
	name_ = name ? std::string(name) : std::string();
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::sizeBits(uint64_t sizeBits)
{
	sizeBits_ = sizeBits;
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::isSigned(bool isSigned)
{
	isSigned_ = isSigned;
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::lsBit(int lsBit)
{
	lsBit_    = lsBit;
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::mode(Mode mode)
{
	mode_     = mode;
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::wordSwap(unsigned wordSwap)
{
	wordSwap_ = wordSwap;
	return getSelf<Builder>();
}

IIntField::Builder CIntEntryImpl::CBuilder::reset()
{
	init();
	return getSelf<Builder>();
}

void CIntEntryImpl::CBuilder::init()
{
	name_     = std::string();
	sizeBits_ = 32;
	lsBit_    = 0;
	isSigned_ = false;
	mode_     = RW;
    wordSwap_ = 0;
}

IntField CIntEntryImpl::CBuilder::build()
{
	return build( name_.c_str() );
}

IntField CIntEntryImpl::CBuilder::build(const char *name)
{
	return CEntryImpl::create<CIntEntryImpl>(name, sizeBits_, isSigned_, lsBit_, mode_, wordSwap_);
}

IIntField::Builder  CIntEntryImpl::CBuilder::clone()
{
shared_ptr<CIntEntryImpl::CBuilder> rval( doClone() );
		rval->setSelf( rval );

		if ( typeid( *rval ) != typeid( *this ) )
			throw InternalError("Some subclass of CIntEntryImpl::CBuilder doesn't implement doClone()");

		return rval;
}

IIntField::Builder  CIntEntryImpl::CBuilder::create()
{
CBuilder *p = new CBuilder();
shared_ptr<CIntEntryImpl::CBuilder> rval( p );
	rval->setSelf( rval );
	return rval;
}

IIntField::Builder IIntField::IBuilder::create()
{
	return CIntEntryImpl::CBuilder::create();
}
