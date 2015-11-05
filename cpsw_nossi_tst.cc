#include <api_user.h>
#include <cpsw_nossi_dev.h>
#include <cpsw_mmio_dev.h>

int
main()
{
NoSsiDev  root("fpga","192.168.2.10");
MMIODev   mmio("mmio",0x100000,0,UNKNOWN);
MMIODev   axiv("vers",0x1000,  0,LE);
MMIODev   sysm("sysm",0x1000,  0,LE);

uint8_t str[30];

	axiv.addAtAddr( new IntEntry("blds", 8, false, 0), 0x800, 20    );

	sysm.addAtAddr( new IntEntry("adcs", 16, true, 0), 0x400, 10, 4 );

	mmio.addAtAddr( &axiv, 0x00000 );
	mmio.addAtAddr( &sysm, 0x10000 );

	root.addAtAddr( &mmio, 8192 );

	ScalVal_RO blds = IScalVal_RO::create( root.findByName("mmio/vers/blds") );
	ScalVal_RO adcs = IScalVal_RO::create( root.findByName("mmio/sysm/adcs") );

	blds->getVal( str, sizeof(str)/sizeof(str[0]) );

// 8192
}
