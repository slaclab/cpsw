#include <api_user.h>
#include <cpsw_path.h>
#include <cpsw_hub.h>

class IEntryAdapt : public virtual IEntry {
protected:
	shared_ptr<const IntEntryImpl> ie;
	Path     p;

	IEntryAdapt(Path p, shared_ptr<const IntEntryImpl> ie)
	:ie(ie), p(p)
	{
		if ( p->empty() )
			throw InvalidPathError("<EMPTY>");
		if ( p->tail()->getEntry() != ie )
			throw InternalError("inconsistent args passed to IEntryAdapt");
		if ( UNKNOWN == p->tail()->getByteOrder() ) {
			throw ConfigurationError("Configuration Error: byte-order not set");
		}
	}
public:
	virtual const char *getName()  const { return ie->getName(); }
	virtual uint64_t    getSize() const { return ie->getSize(); }
};

class IIntEntryAdapt : public IEntryAdapt {
public:
	IIntEntryAdapt(Path p, shared_ptr<const IntEntryImpl> ie) : IEntryAdapt(p, ie) {}
	virtual bool     isSigned()    const { return ie->isSigned();    }
	virtual int      getLsBit()    const { return ie->getLsBit();    }
	virtual uint64_t getSizeBits() const { return ie->getSizeBits(); }
	virtual unsigned getWordSwap() const { return ie->getWordSwap(); }
};

class ScalVal_ROAdapt : public IScalVal_RO, public IIntEntryAdapt {
private:
	int nelms;
public:
	ScalVal_ROAdapt(Path p, shared_ptr<const IntEntryImpl> ie)
	: IIntEntryAdapt(p, ie), nelms(-1)
	{
	}

	virtual bool     isSigned()    const { return IIntEntryAdapt::isSigned(); }
	virtual uint64_t getSizeBits() const { return IIntEntryAdapt::getSizeBits(); }
	virtual int      getNelms();

	virtual unsigned getVal(uint8_t  *, unsigned, unsigned);

	virtual unsigned getVal(uint64_t *, unsigned);
	virtual unsigned getVal(uint32_t *, unsigned);
	virtual unsigned getVal(uint16_t *, unsigned);
	virtual unsigned getVal(uint8_t  *, unsigned);
};

template <typename E, typename A> static ScalVal_RO check_interface(Path p)
{
shared_ptr<const E> e = boost::dynamic_pointer_cast<const E, EntryImpl>( p->tail()->getEntry() );
	if ( e ) {
		A *a_p = new A(p, e);
		return ScalVal_RO( a_p );
	}
	throw InterfaceNotImplementedError( p );
}

ScalVal_RO IScalVal_RO::create(Path p)
{
	// could try other implementations of this interface here
	return check_interface<IntEntryImpl, ScalVal_ROAdapt>( p );
}

int ScalVal_ROAdapt::getNelms()
{
	if ( nelms < 0 ) {
		CompositePathIterator it( & p );
		while ( ! it.atEnd() )
			++it;
		nelms = it.getNelmsRight();
	}
	return nelms;	
}

unsigned ScalVal_ROAdapt::getVal(uint8_t *buf, unsigned nelms, unsigned elsz)
{
CompositePathIterator it( & p );
Child            cl = it->c_p;
uint64_t         off = 0;
unsigned         sbytes   = IEntryAdapt::getSize(); // byte-size including lsb shift
unsigned         nbytes   = (getSizeBits() + 7)/8;  // byte-size w/o lsb shift
unsigned         dbytes   = elsz;
unsigned         ibuf_nchars;
int              lsb      = getLsBit();
ByteOrder        host     = hostByteOrder();

	if ( (unsigned)getNelms() <= nelms )
		nelms = getNelms();
	else
		throw InvalidArgError("Invalid Argument: buffer too small");

	if ( sbytes > dbytes )
		ibuf_nchars = sbytes * nelms;
	else
		ibuf_nchars = 0;

	uint8_t ibuf[ibuf_nchars];

	uint8_t *ibufp = ibuf_nchars ? ibuf : (uint8_t*)buf;
	uint8_t *obufp = (uint8_t*)buf;

	if ( dbytes > sbytes ) {
		// obuf and ibuf overlap;
		if ( BE == host ) {
			// work top down
			ibufp += (dbytes-sbytes) * nelms;
		}
	}
	
	cl->read( &it, ie->getCacheable(), ibufp, sbytes, off, sbytes );

	bool sign_extend = getSizeBits() < 8*dbytes;
	bool truncate    = getSizeBits() > 8*dbytes;

	unsigned wlen    = getWordSwap();

	if (   cl->getByteOrder() != host
		|| lsb                != 0
	    || sign_extend
		|| truncate
		|| wlen               >  0 ) {

		// transformation necessary

		int nwrds   = wlen ? nbytes/wlen : 0;
		int ioff = (BE == host ? sbytes - dbytes : 0 );
		int noff = (BE == host ? sbytes - nbytes : 0 );
		int ooff;
		int iidx, oidx, n, iinc, oinc;
		if ( ioff < 0 ) {
			ooff = -ioff;
			ioff = 0;
		} else {
			ooff = 0;
		}

		uint8_t is_signed = isSigned();
		int     signByte  = (getSizeBits()-1)/8;
		uint8_t signBit   = (1<<((getSizeBits()-1) & 7));
		uint8_t signmsk   = signBit | (signBit - 1); 

		if ( BE == host ) {
			// top down	
			oidx = 0;
			iidx = 0;
			iinc = +sbytes;
			oinc = +dbytes;
			signByte = dbytes - 1 - signByte;
		} else {
			// bottom up
			oidx = (nelms-1)*dbytes;
			iidx = (nelms-1)*sbytes;
			iinc = -sbytes;
			oinc = -dbytes;
		}
		for ( n = nelms-1; n >= 0; n--, oidx += oinc, iidx += iinc ) {

			if ( cl->getByteOrder() != host ) {
				for ( unsigned j = 0; j<sbytes/2; j++ ) {
					uint8_t tmp = ibufp[iidx + j];
					ibufp[iidx + j] = ibufp[iidx + sbytes - 1 - j];
					ibufp[iidx + sbytes - 1 - j] = tmp;
				}
			}

			if ( lsb != 0 ) {
                uint16_t tmp16;
				int      j;
				if ( LE == host ) {
					tmp16 = 0;
					for ( j = sbytes - 1; j >= 0; j-- ) {
						tmp16 = (tmp16<<8) | ibufp[iidx + j];
//printf("j %i, oidx %i, iidx %i, tmp16 %04x\n", j, oidx, iidx, tmp16);
						ibufp[iidx+j] = (tmp16 >> lsb);
					}
				} else {
					tmp16 = 0;
					for (  j = 0; j < (int)sbytes; j++ ) {
						tmp16 = (tmp16<<8) | ibufp[iidx + j];
//printf("j %i, oidx %i, iidx %i, tmp16 %04x\n", j, oidx, iidx, tmp16);
						ibufp[iidx + j] = (tmp16 >> lsb);
					}
				}
			}

			if ( wlen  > 0 ) {
				int j, jinc;
//printf("pre-wswap (nbytes %i, iidx %i): ", nbytes, iidx);
//for(j=0;j<sbytes;j++)
//	printf("%02x ", ibufp[iidx+j]);
//printf("\n");
				for ( j=jinc=0; j<nwrds/2; j++, jinc+=wlen  ) {
					uint8_t tmp[wlen];
					memcpy(tmp,                                        ibufp + iidx + noff + jinc, wlen);
					memcpy(ibufp + iidx + noff + jinc, ibufp + iidx + noff + nbytes - wlen - jinc, wlen);
					memcpy(ibufp + iidx + noff + nbytes - wlen - jinc,                        tmp, wlen);
				}
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
				int jinc = (LE == host ?     +1 : -1);
				int jend = (LE == host ? dbytes : -1);
				int j    = signByte;
//printf("SE signByte %i, signBit %x\n", signByte, signBit);
				if ( is_signed && (obufp[oidx + signByte] & signBit) != 0 ) {
					obufp[oidx + j] |= ~signmsk;
					j += jinc;
					while ( j != jend ) {
						obufp[oidx + j] = 0xff;
						j += jinc;
					}
				} else {
					obufp[oidx + j] &=  signmsk;
					j += jinc;
					while ( j != jend ) {
						obufp[oidx + j] = 0x00;
						j += jinc;
					}
				}
//for ( j=0; j <dbytes; j++ ) printf("obuf[%i] 0x%02x ", j, obufp[oidx+j]);  printf("\n");
			}
		}
	}
	return nelms;
}

unsigned ScalVal_ROAdapt::getVal(uint8_t *buf, unsigned nelms)
{
	return ScalVal_ROAdapt::getVal(buf, nelms, sizeof(*buf));
}

unsigned ScalVal_ROAdapt::getVal(uint16_t *buf, unsigned nelms)
{
	return ScalVal_ROAdapt::getVal((uint8_t*)buf, nelms, sizeof(*buf));
}

unsigned ScalVal_ROAdapt::getVal(uint32_t *buf, unsigned nelms)
{
	return ScalVal_ROAdapt::getVal((uint8_t*)buf, nelms, sizeof(*buf));
}

unsigned ScalVal_ROAdapt::getVal(uint64_t *buf, unsigned nelms)
{
	return ScalVal_ROAdapt::getVal((uint8_t*)buf, nelms, sizeof(*buf));
}
