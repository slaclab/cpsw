#include <api_user.h>
#include <cpsw_nossi_dev.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_mem_dev.h>
#include <string.h>
#include <inttypes.h>

#define VLEN 123
#define ADCL 10

#ifndef PRIx64
#define PRIx64 "lx"
#endif
#ifndef PRIx32
#define PRIx32 "x"
#endif

class AXIV;

class SWPWAddress : public MMIOAddress {
	protected:
		SWPWAddress(AXIV *owner, Entry *child, uint64_t offset, unsigned nelms, unsigned stride);
		friend class AXIV;
	public:
		virtual uint64_t  read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
};

class AXIV : public MMIODev {

public:
	AXIV(const char *name) : MMIODev(name, 0x1000, 0, LE)
	{
		addAtSWPWAddr( new IntEntry("dnaValue", 64, false, 0), 0x08 );
		addAtSWPWAddr( new IntEntry("fdSerial", 64, false, 0), 0x10 );
		addAtAddr(     new IntEntry("counter",  32, false, 0), 0x24 );
		addAtAddr(     new IntEntry("bldStamp",  8, false, 0), 0x800, VLEN  );
	}

	virtual void addAtSWPWAddr(Entry *child, uint64_t offset, unsigned nelms = 1, uint64_t stride = -1ULL) {
		Address *a = new SWPWAddress(this, child, offset, nelms, stride);
		add( a, child );
	}
};

SWPWAddress::SWPWAddress(AXIV *owner, Entry *child, uint64_t offset, unsigned nelms, unsigned stride)
:MMIOAddress(owner, child, offset, nelms, stride)
{
}

uint64_t  SWPWAddress::read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int wlen = 4;
int nw = sbytes / wlen;
int i,iind;
uint8_t tmp[wlen];

	if ( (sbytes % wlen) != 0 )
		throw ConfigurationError("misaligned address");

	MMIOAddress::read(node, cacheable, dst, dbytes ,off, sbytes);

	/* word-swap */
	for ( iind=i=0; i<nw/2; i++, iind+=wlen ) {
		memcpy( tmp                     , dst + iind              , wlen);
		memcpy( dst + iind              , dst + (sbytes-wlen-iind), wlen);
		memcpy( dst + (sbytes-wlen-iind), tmp                     , wlen);
	}

	return sbytes;
}

int
main()
{
NoSsiDev  root("fpga","192.168.2.10");
MMIODev   mmio("mmio",0x100000,0,UNKNOWN);
AXIV      axiv("vers");
MMIODev   sysm("sysm",0x1000,  0,LE);
MemDev    rmem("rmem", 0x100000);

uint8_t str[VLEN];
int16_t adcv[ADCL];
uint64_t u64;
uint32_t u32;

	rmem.addAtAddr( &mmio );

	for (int i=16; i<24; i++ )
		rmem.buf[i]=i-16;


	sysm.addAtAddr( new IntEntry("adcs", 16, true, 0), 0x400, ADCL, 4 );

	mmio.addAtAddr( &axiv, 0x00000 );

	mmio.addAtAddr( &sysm, 0x10000 );

	root.addAtAddr( &mmio, 8192 );

	Path pre = IPath::create( &root );


	ScalVal_RO bldStamp = IScalVal_RO::create( pre->findByName("mmio/vers/bldStamp") );
	ScalVal_RO fdSerial = IScalVal_RO::create( pre->findByName("mmio/vers/fdSerial") );
	ScalVal_RO dnaValue = IScalVal_RO::create( pre->findByName("mmio/vers/dnaValue") );
	ScalVal_RO counter  = IScalVal_RO::create( pre->findByName("mmio/vers/counter" ) );

	ScalVal_RO adcs = IScalVal_RO::create( pre->findByName("mmio/sysm/adcs") );

	bldStamp->getVal( str, sizeof(str)/sizeof(str[0]) );

	printf("Build String:\n%s\n", (char*)str);
	fdSerial->getVal( &u64, 1 );
	printf("Serial #: 0x%"PRIx64"\n", u64);
	dnaValue->getVal( &u64, 1 );
	printf("DNA    #: 0x%"PRIx64"\n", u64);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%"PRIx32"\n", u32);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%"PRIx32"\n", u32);

	adcs->getVal( (uint16_t*)adcv, sizeof(adcv)/sizeof(adcv[0]) );
	printf("\n\nADC Values:\n");
	for ( int i=0; i<ADCL; i++ ) {
		printf("  %hd\n", adcv[i]);
	}

// 8192
}
