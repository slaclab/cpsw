#include <api_user.h>
#include <cpsw_nossi_dev.h>
#include <cpsw_mmio_dev.h>

#define VLEN 128
#define ADCL 10

class AXIV : public MMIODev {
public:
	AXIV(const char *name) : MMIODev(name, 0x1000, 0, LE)
	{
		addAtAddr( new IntEntry("blds", 8, false, 0), 0x800, VLEN  );
	}
};

int
main()
{
NoSsiDev  root("fpga","192.168.2.10");
MMIODev   mmio("mmio",0x100000,0,UNKNOWN);
AXIV      axiv("vers");
MMIODev   sysm("sysm",0x1000,  0,LE);

uint8_t str[VLEN];
int16_t adcv[ADCL];


	sysm.addAtAddr( new IntEntry("adcs", 16, true, 0), 0x400, ADCL, 4 );

	mmio.addAtAddr( &axiv, 0x00000 );
	mmio.addAtAddr( &sysm, 0x10000 );

	root.addAtAddr( &mmio, 8192 );

	ScalVal_RO blds = IScalVal_RO::create( root.findByName("mmio/vers/blds") );
	ScalVal_RO adcs = IScalVal_RO::create( root.findByName("mmio/sysm/adcs") );

	blds->getVal( str, sizeof(str)/sizeof(str[0]) );
	adcs->getVal( (uint16_t*)adcv, sizeof(adcv)/sizeof(adcv[0]) );

	printf("Build String:\n%s\n", (char*)str);

	printf("\n\nADC Values:\n");
	for ( int i=0; i<ADCL; i++ ) {
		printf("  %hd\n", adcv[i]);
	}

// 8192
}
