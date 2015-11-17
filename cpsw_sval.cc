#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_hub.h>

class IEntryAdapt : public virtual IEntry {
protected:
	shared_ptr<const CIntEntryImpl> ie;
	Path     p;

	IEntryAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	:ie(ie), p(p->clone())
	{
		if ( p->empty() )
			throw InvalidPathError("<EMPTY>");

		Address  a = CompositePathIterator( &p )->c_p;

		if ( a->getEntry() != ie )
			throw InternalError("inconsistent args passed to IEntryAdapt");
		if ( UNKNOWN == a->getByteOrder() ) {
			throw ConfigurationError("Configuration Error: byte-order not set");
		}
	}
public:
	virtual const char *getName()        const { return ie->getName(); }
	virtual const char *getDescription() const { return ie->getDescription(); }
	virtual uint64_t    getSize()        const { return ie->getSize(); }
};

class IIntEntryAdapt : public IEntryAdapt, public virtual IScalVal_Base {
private:
	int nelms;
public:
	IIntEntryAdapt(Path p, shared_ptr<const CIntEntryImpl> ie) : IEntryAdapt(p, ie), nelms(-1) {}
	virtual bool     isSigned()    const { return ie->isSigned();    }
	virtual int      getLsBit()    const { return ie->getLsBit();    }
	virtual uint64_t getSizeBits() const { return ie->getSizeBits(); }
	virtual unsigned getWordSwap() const { return ie->getWordSwap(); }
	virtual unsigned getNelms();
};

class ScalVal_ROAdapt : public virtual IScalVal_RO, public virtual IIntEntryAdapt {
public:
	ScalVal_ROAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
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

class ScalVal_WOAdapt : public virtual IScalVal_WO, public virtual IIntEntryAdapt {
public:
	ScalVal_WOAdapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie)
	{
	}

	template <typename E> unsigned setVal(E *e, unsigned nelms) {
		return setVal( reinterpret_cast<uint8_t*>(e), nelms, sizeof(E) );
	}

	virtual unsigned setVal(uint8_t  *, unsigned, unsigned);

	virtual unsigned setVal(uint64_t *p, unsigned n) { return setVal<uint64_t>(p,n); }
	virtual unsigned setVal(uint32_t *p, unsigned n) { return setVal<uint32_t>(p,n); }
	virtual unsigned setVal(uint16_t *p, unsigned n) { return setVal<uint16_t>(p,n); }
	virtual unsigned setVal(uint8_t  *p, unsigned n) { return setVal<uint8_t> (p,n); }

	virtual unsigned setVal(uint64_t  v) { throw InternalError("FIXME"); }
};

class ScalVal_Adapt : public virtual ScalVal_ROAdapt, public virtual ScalVal_WOAdapt, public virtual IScalVal {
public:
	ScalVal_Adapt(Path p, shared_ptr<const CIntEntryImpl> ie)
	: IIntEntryAdapt(p, ie), ScalVal_ROAdapt(p, ie), ScalVal_WOAdapt(p, ie)
	{
	}
};

template <typename EIMPL, typename IMPL, typename IFAC> static IFAC check_interface(Path p)
{
	if ( p->empty() )
		throw InvalidArgError("Empty Path");

	Address a = CompositePathIterator( &p )->c_p;
	shared_ptr<const EIMPL> e = boost::dynamic_pointer_cast<const EIMPL, CEntryImpl>( a->getEntryImpl() );
	if ( e ) {
		return IFAC( make_shared<IMPL>(p, e) );
	}
	throw InterfaceNotImplementedError( p );
}

ScalVal_RO IScalVal_RO::create(Path p)
{
	// could try other implementations of this interface here
	return check_interface<CIntEntryImpl, ScalVal_ROAdapt, ScalVal_RO>( p );
}

ScalVal_WO IScalVal_WO::create(Path p)
{
	// could try other implementations of this interface here
	return check_interface<CIntEntryImpl, ScalVal_WOAdapt, ScalVal_WO>( p );
}

ScalVal IScalVal::create(Path p)
{
	// could try other implementations of this interface here
	return check_interface<CIntEntryImpl, ScalVal_Adapt, ScalVal>( p );
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

		//printf("SE signByte %i, signBit %x\n", signByte, signBit);
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


unsigned ScalVal_ROAdapt::getVal(uint8_t *buf, unsigned nelms, unsigned elsz)
{
CompositePathIterator it( & p );
Address          cl        = it->c_p;
uint64_t         off       = 0;
unsigned         sbytes    = IEntryAdapt::getSize(); // byte-size including lsb shift
unsigned         nbytes    = (getSizeBits() + 7)/8;  // byte-size w/o lsb shift
unsigned         dbytes    = elsz;
unsigned         ibuf_nchars;
int              lsb       = getLsBit();
ByteOrder        hostEndian= hostByteOrder();
ByteOrder        targetEndian= cl->getByteOrder();

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

#if 0
static void prib(uint8_t *b)
{
for (int i=0; i<9; i++ ) {
	printf("%02x ", b[i]);
}
	printf("\n");
}
#endif

unsigned ScalVal_WOAdapt::setVal(uint8_t *buf, unsigned nelms, unsigned elsz)
{
CompositePathIterator it( & p );
Address          cl = it->c_p;
uint64_t         off = 0;
unsigned         dbytes   = IEntryAdapt::getSize(); // byte-size including lsb shift
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

		for ( n = nelms-1; n >= 0; n--, oidx += oinc, iidx += iinc ) {

			memcpy( obufp + oidx + ooff, ibufp + iidx + ioff, dbytes >= sbytes ? sbytes : dbytes );

			if ( sign_extend ) {
				signExtend.work(obufp + oidx + ooff, obufp + oidx + ooff);
			}

			if ( wlen  > 0 ) {
				wordSwap.work( obufp + oidx + noff );
			}

			if ( lsb != 0 ) {
				bits.shiftLeft( obufp + oidx );
			}

			if ( targetEndian != hostEndian ) {
				byteSwap.work( obufp + oidx );
			}
		}
	}

	cl->write( &it, ie->getCacheable(), obufp, dbytes, off, dbytes, msk1, mskn );

	return nelms;
}
