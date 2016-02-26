#ifndef UDPSRV_REGDEFS_H
#define UDPSRV_REGDEFS_H

#define MEM_ADDR 0x00000000
#define MEM_SIZE (1024*1024)

#define REGBASE 0x1000 /* pseudo register space */
#define REG_RO_OFF 0
#define REG_RO_SZ 16
#define REG_CLR_OFF REG_RO_SZ
#define REG_SCR_OFF (REG_CLR_OFF + 8)
#define REG_SCR_SZ  32
#define REG_ARR_OFF (REG_SCR_OFF + REG_SCR_SZ)
#define REG_ARR_SZ  8192

#define AXI_SPI_EEPROM_ADDR (MEM_ADDR + MEM_SIZE)
#define AXI_SPI_EEPROM_32BIT_MODE_OFF 0x04
#define AXI_SPI_EEPROM_ADDR_OFF       0x08
#define AXI_SPI_EEPROM_CMD_OFF        0x0c
#define AXI_SPI_EEPROM_DATA_OFF      0x200

#define AXI_SPI_EEPROM_SIZE (1024)

#define DEBUG

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

struct udpsrv_range;

extern
#ifdef __cplusplus
"C"
#endif
struct udpsrv_range *udpsrv_ranges;

struct udpsrv_range {
	struct udpsrv_range *next;
	uint32_t base;
	uint32_t size;
	int    (*read) (uint32_t *data, uint32_t nwrds, uint32_t off, int debug);
	int    (*write)(uint32_t *data, uint32_t nwrds, uint32_t off, int debug);
#ifdef __cplusplus
	udpsrv_range(
		uint32_t base,
		uint32_t size,
		int    (*read)(uint32_t *data, uint32_t nwrds, uint32_t off, int debug),
		int    (*write)(uint32_t *data, uint32_t nwrds, uint32_t off, int debug),
		void   (*init)()
	)
	:next(udpsrv_ranges), base(base), size(size), read(read), write(write)
	{
		udpsrv_ranges = this;
		if ( init )
			init();
	}
#endif
};

#endif

