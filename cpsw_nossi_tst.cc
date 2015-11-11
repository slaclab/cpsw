#include <api_builder.h>
#include <cpsw_mmio_dev.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <getopt.h>

#define VLEN 123
#define ADCL 10

#ifndef PRIx64
#define PRIx64 "lx"
#endif
#ifndef PRIx32
#define PRIx32 "x"
#endif

class   IAXIVers;
typedef shared_ptr<IAXIVers> AXIVers;

class IAXIVers : public virtual IMMIODev {
public:
	virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO) = 0;

	static AXIVers create(const char *name);
};

class SWPWAddressImpl : public MMIOAddressImpl {
	public:
		SWPWAddressImpl(AKey key, uint64_t offset, unsigned nelms, unsigned stride);
		virtual uint64_t  read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
};

class AXIVersImpl : public MMIODevImpl, public virtual IAXIVers {

public:
	AXIVersImpl(FKey);

	virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO) {
		AKey key = getAKey();
		add( make_shared<SWPWAddressImpl>(key, offset, nelms, stride), child );
	}

};

AXIVers IAXIVers::create(const char *name)
{
shared_ptr<AXIVersImpl> v = EntryImpl::create<AXIVersImpl>(name);
Field f;
	f = IIntField::create("dnaValue", 64, false, 0);
	v->AXIVersImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("fdSerial", 64, false, 0);
	v->AXIVersImpl::addAtAddress( f, 0x10 );
	f = IIntField::create("counter",  32, false, 0);
	v->MMIODevImpl::addAtAddress( f, 0x24 );
	f = IIntField::create("bldStamp",  8, false, 0);
	v->MMIODevImpl::addAtAddress( f, 0x800, VLEN  );
	return v;	
}

AXIVersImpl::AXIVersImpl(FKey key) : MMIODevImpl(key, 0x1000, LE)
{
}

SWPWAddressImpl::SWPWAddressImpl(AKey key, uint64_t offset, unsigned nelms, unsigned stride)
:MMIOAddressImpl(key, offset, nelms, stride)
{
}

uint64_t  SWPWAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
int wlen = 4;
int nw = sbytes / wlen;
int i,iind;
uint8_t tmp[wlen];

	if ( (sbytes % wlen) != 0 )
		throw ConfigurationError("misaligned address");

	MMIOAddressImpl::read(node, cacheable, dst, dbytes ,off, sbytes);

	/* word-swap */
	for ( iind=i=0; i<nw/2; i++, iind+=wlen ) {
		memcpy( tmp                     , dst + iind              , wlen);
		memcpy( dst + iind              , dst + (sbytes-wlen-iind), wlen);
		memcpy( dst + (sbytes-wlen-iind), tmp                     , wlen);
	}

	return sbytes;
}

int
main(int argc, char **argv)
{
NoSsiDev  root = INoSsiDev::create("fpga","192.168.2.10");
MMIODev   mmio = IMMIODev::create ("mmio",0x100000);
AXIVers   axiv = IAXIVers::create("vers");
MMIODev   sysm = IMMIODev::create ("sysm",0x1000, LE);
MemDev    rmem = IMemDev::create  ("rmem", 0x100000);

int       use_mem = 0;
int       ch;

uint8_t str[VLEN];
int16_t adcv[ADCL];
uint64_t u64;
uint32_t u32;

	while ( (ch = getopt(argc, argv, "m")) > 0 ) {
		switch (ch) {
			case 'm': use_mem = 1; break;
			default:
				printf("Unknown option '%c'\n", ch);
				exit(1);
		}
	}

	rmem->addAtAddress( mmio );

	uint8_t *buf = rmem->getBufp();
	for (int i=16; i<24; i++ )
		buf[i]=i-16;


	sysm->addAtAddress( IIntField::create("adcs", 16, true, 0), 0x400, ADCL, 4 );

	mmio->addAtAddress( axiv, 0x00000 );

	mmio->addAtAddress( sysm, 0x10000 );

	root->addAtAddress( mmio, 8192 );

	// can use raw memory for testing instead of UDP
	Path pre = use_mem ? IPath::create( rmem ) : IPath::create( root );

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
	return 0;
}
