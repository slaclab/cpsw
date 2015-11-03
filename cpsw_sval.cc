#include <api_user.h>
#include <cpsw_path.h>
#include <cpsw_hub.h>

template<typename E> class IEntryAdapt : public virtual IEntry {
protected:
	const E *ie;
	Path     p;
public:
	IEntryAdapt(Path p)
	:p(p)
	{
		if ( p->empty() )
			throw InvalidPathError("<EMPTY>");
		if ( ! (ie = dynamic_cast<const E *>( p->tail() )) ) {
			throw InterfaceNotImplementedError(p);
		}
		if ( UNKNOWN == ie->getByteOrder() ) {
			throw ConfigurationError("Configuration Error: byte-order not set");
		}
	}
	virtual const char *getName()  const { return ie->getName(); }
	virtual uint64_t    getSize() const { return ie->getSize(); }
};

template<typename E> class IIntEntryAdapt : public IEntryAdapt<E> {
public:
	IIntEntryAdapt(Path p) : IEntryAdapt<E>(p) {}
	virtual bool     isSigned()    const { return IEntryAdapt<E>::ie->isSigned();    }
	virtual int      getLsBit()    const { return IEntryAdapt<E>::ie->getLsBit();    }
	virtual uint64_t getSizeBits() const { return IEntryAdapt<E>::ie->getSizeBits(); }
	using IEntryAdapt<E>::getName;
};

template<typename E> class ScalVal_ROAdapt : public IScalVal_RO, public IIntEntryAdapt<E> {
private:
	int nelms;
public:
	ScalVal_ROAdapt(Path p)
	: IIntEntryAdapt<E>(p), nelms(-1)
	{
	}

	virtual bool     isSigned()    const { return isSigned(); }
	virtual int      getLsBit()    const { return getLsBit(); }
	virtual uint64_t getSizeBits() const { return getSizeBits(); }
	virtual int      getNelms();
	virtual unsigned getVal(uint64_t *, unsigned);
	virtual unsigned getVal(uint32_t *, unsigned);
	virtual unsigned getVal(uint16_t *, unsigned);
	virtual unsigned getVal(uint8_t  *, unsigned);
};

ScalVal_RO IScalVal_RO::create(Path p) {
	return ScalVal_RO( new ScalVal_ROAdapt<IntEntry>( p ) );
}

template <typename E> int ScalVal_ROAdapt<E>::getNelms()
{
	if ( nelms < 0 ) {
		CompositePathIterator it( & (IEntryAdapt<E>::p) );
		while ( ! it.atEnd() )
			++it;
		nelms = it.getNelmsRight();
	}
	return nelms;	
}

template <typename E> unsigned ScalVal_ROAdapt<E>::getVal(uint16_t *buf, unsigned nelms)
{
const    IntEntry *ie = IEntryAdapt<E>::ie;
CompositePathIterator it( & (IEntryAdapt<E>::p) );
const Child       *cl = it->c_p;
uint64_t         got;
uint64_t         off;
unsigned         sbytes   = getSize();
unsigned         dbytes   = sizeof(*buf);
unsigned         ibuf_nchars;
int              lsb      = getLsBit();

	if ( (unsigned)getNelms() <= nelms )
		nelms = getNelms();
	else
		throw InvalidArgError("Invalid Argument: buffer too small");

	if ( sbytes > dbytes && ! ie->getCacheable() )
		ibuf_nchars = sbytes * nelms;
	else
		ibuf_nchars = 0;

	uint8_t ibuf[ibuf_nchars];

	uint8_t *ibufp = ibuf_nchars ? ibuf : (uint8_t*)buf;
	uint8_t *obufp = (uint8_t*)buf;

	if (   ie->getByteOrder() != hostByteOrder
		|| lsb                != 0
	    || getSizeBits()      != 8*dbytes  ) {
		// transformation necessary
		int ioff = (BE == hostByteOrder ? sbytes - dbytes : 0 );
		int ooff;
		int iidx, oidx;
		if ( ioff < 0 ) {
			ooff = -ioff;
			ioff = 0;
		} else {
			ooff = 0;
		}
		for ( oidx = (nelms-1)*dbytes, iidx = (nelms-1)*sbytes; oidx >= 0; oidx -= dbytes, iidx -= sbytes ) {
			if ( ie->getByteOrder() != hostByteOrder ) {
				for ( int j = 0; j<sbytes; j++ ) {
					uint8_t tmp = ibufp[iidx + j];
					ibufp[iidx + j] = ibufp[iidx + sbytes - 1 - j];
					ibufp[iidx + sbytes - 1 - j] = tmp;
				}
			}

			if ( lsb != 0 ) {
                uint16_t tmp16;
				if ( LE == hostByteOrder ) {
					for ( int j = (dbytes > sbytes ? sbytes : dbytes) - 1, tmp16=0; j>=0; j--, tmp16 <<= 8 ) {
						tmp16        |= ibufp[iidx + j];
						obufp[oidx+j] = (tmp16 >> lsb);
					}
				} else {
					for ( int j = (dbytes > sbytes ? sbytes : dbytes) - 1, tmp16=0; j>0; j--, tmp16 <<= 8 ) {
						tmp16             |= ibufp[iidx + ioff + j - 1];
						obufp[oidx+ooff+j] = (tmp16 >> lsb);
					}
				}
			} else {
				
			}
		}
	}

/*
      headbits 2  size 9

      xx987654 321xxxxx

      LE       BE
      321xxxxx xx987654
      xx987654 321xxxxx


      read
      bbbbbbbb bxxxxxxx

      be                    le

      bbbbbbbb bxxxxxxx     bbbbbbbb b 
     
      s > d
      BE             LE
      s3 s2 s1 s0    s0 s1 s2 s3
            d1 d0    d0 d1

      s < d
            s1 s0    s0 s1
      d3 d2 d1 d0    d0 d1 d2 d3
 */

	got = cl->read( &it, ie->getCacheable(), ibufp, dbytes, off, sbytes );

}
