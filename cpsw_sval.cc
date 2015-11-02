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
	virtual uint64_t getSizeBits() const { return ie->getSizeBits(); }
};

template<typename E> class IIntEntryAdapt : public IEntryAdapt<E> {
public:
	IIntEntryAdapt(Path p) : IEntryAdapt<E>(p) {}
	virtual bool isSigned() const { return IEntryAdapt<E>::ie->isSigned(); }
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

	virtual bool     isSigned() const { return isSigned(); }
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
int              headBits ;
uint64_t         sizeBits = getSizeBits();

	if ( (unsigned)getNelms() <= nelms )
		nelms = getNelms();
	else
		throw InvalidArgError("Invalid Argument: buffer too small");

	if ( sizeBits > 8*sizeof(*buf) ) {
		if ( BE == ie->getByteOrder() ) {
			headBits += (sizeBits - 8*sizeof(*buf));
			off += headBits/8;
			headBits  = headBits & 7;
		}
		sizeBits = 8*sizeof(*buf);
	}

	got = cl->read( &it, (uint8_t*)buf, off, headBits, sizeBits );

	if ( ie->getByteOrder() != hostByteOrder ) {
		for ( int i = 0; i<nelms; i++ ) {
		}
	}
}
