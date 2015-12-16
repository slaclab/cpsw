#include <cpsw_api_builder.h>
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

class TestFailed {};

class   IAXIVers;
typedef shared_ptr<IAXIVers> AXIVers;

class CAXIVersImpl;
typedef shared_ptr<CAXIVersImpl> AXIVersImpl;

class IAXIVers : public virtual IMMIODev {
public:
	static AXIVers create(const char *name);
};

class CAXIVersImpl : public CMMIODevImpl, public virtual IAXIVers {
public:
	CAXIVersImpl(FKey);
};

AXIVers IAXIVers::create(const char *name)
{
AXIVersImpl v = CEntryImpl::create<AXIVersImpl::element_type>(name);
Field f;
	f = IIntField::create("dnaValue", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("fdSerial", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f, 0x10 );
	f = IIntField::create("counter",  32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f, 0x24 );
	f = IIntField::create("bldStamp",  8, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f, 0x800, VLEN  );
	f = IIntField::create("scratchPad",32,true,  0);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	f = IIntField::create("bits",22,true, 4);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	return v;	
}

CAXIVersImpl::CAXIVersImpl(FKey key) : CMMIODevImpl(key, 0x1000, LE)
{
}

int
main(int argc, char **argv)
{
const char *ip_addr = "192.168.2.10";
bool        use_mem = false;
int        *i_p;
int         vers    = 2;

	for ( int opt; (opt = getopt(argc, argv, "a:mV:")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr = optarg; break;
			case 'm': use_mem = true;   break;
			case 'V': i_p     = &vers;  break;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( vers != 1 && vers != 2 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1 or 2\n", vers);
		throw TestFailed();
	}

try {

NoSsiDev  root = INoSsiDev::create("fpga", ip_addr);
MMIODev   mmio = IMMIODev::create ("mmio",0x100000);
AXIVers   axiv = IAXIVers::create ("vers");
MMIODev   sysm = IMMIODev::create ("sysm",0x1000, LE);
MemDev    rmem = IMemDev::create  ("rmem", 0x100000);

uint8_t str[VLEN];
int16_t adcv[ADCL];
uint64_t u64;
uint32_t u32;
uint16_t u16;

	rmem->addAtAddress( mmio );

	uint8_t *buf = rmem->getBufp();
	for (int i=16; i<24; i++ )
		buf[i]=i-16;


	sysm->addAtAddress( IIntField::create("adcs", 16, true, 0), 0x400, ADCL, 4 );

	mmio->addAtAddress( axiv, 0x00000 );
	mmio->addAtAddress( sysm, 0x10000 );

	root->addAtAddress( mmio, 1 == vers ? INoSsiDev::SRP_UDP_V1 : INoSsiDev::SRP_UDP_V2, 8192 );

	// can use raw memory for testing instead of UDP
	Path pre = use_mem ? IPath::create( rmem ) : IPath::create( root );

	ScalVal_RO bldStamp = IScalVal_RO::create( pre->findByName("mmio/vers/bldStamp") );
	ScalVal_RO fdSerial = IScalVal_RO::create( pre->findByName("mmio/vers/fdSerial") );
	ScalVal_RO dnaValue = IScalVal_RO::create( pre->findByName("mmio/vers/dnaValue") );
	ScalVal_RO counter  = IScalVal_RO::create( pre->findByName("mmio/vers/counter" ) );
	ScalVal  scratchPad = IScalVal::create   ( pre->findByName("mmio/vers/scratchPad") );
	ScalVal        bits = IScalVal::create   ( pre->findByName("mmio/vers/bits") );


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
		printf("  %6hd\n", adcv[i]);
	}

	u16=0x6765;
	u32=0xffffffff;
	scratchPad->setVal( &u32, 1 );
	bits->setVal( &u16, 1 );
	scratchPad->getVal( &u32, 1 );
	printf("ScratchPad: 0x%"PRIx32"\n", u32);

	if ( u32 == 0xfc06765f ) {
		printf("Readback of merged bits (expected 0xfc06765f) PASSED\n");
	} else {
		printf("Readback of merged bits (expected 0xfc06765f) FAILED\n");
		throw TestFailed();	
	}
} catch (CPSWError &e) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	return 0;
}