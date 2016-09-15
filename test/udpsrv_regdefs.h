/*
 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
*/

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
#define REG_STRM_OFF (REG_ARR_OFF + REG_ARR_SZ)
#define REG_STRM_SZ  4

#define AXI_SPI_EEPROM_ADDR (MEM_ADDR + MEM_SIZE) /* 0x00100000 */
#define AXI_SPI_EEPROM_32BIT_MODE_OFF 0x04
#define AXI_SPI_EEPROM_ADDR_OFF       0x08
#define AXI_SPI_EEPROM_CMD_OFF        0x0c
#define AXI_SPI_EEPROM_DATA_OFF      0x200

#define AXI_SPI_EEPROM_SIZE (1024)

#define PENDULUM_SIMULATOR_ADDR (AXI_SPI_EEPROM_ADDR + AXI_SPI_EEPROM_SIZE) /* 0x00100400 */
/* Parameters; */
#define PENDULUM_SIMULATOR_AV_OFF       0x00   /* fractional * 2^32 */
#define PENDULUM_SIMULATOR_AF_OFF       0x04   /* fractional * 2^32 */
#define PENDULUM_SIMULATOR_W2_OFF       0x08   /* fractional * 2^24 */
#define PENDULUM_SIMULATOR_NU_OFF       0x0c   /* fractional * 2^16 */
#define PENDULUM_SIMULATOR_LE_OFF       0x10   /* fractional * 2^32 */

/* setpoints; simulator restarted when PH is written */
#define PENDULUM_SIMULATOR_DX_OFF       0x20   /* fractional * 2^31 (signed) */
#define PENDULUM_SIMULATOR_PH_OFF       0x24   /* fractional * 2^31 (signed); normalized to 2PI */
#define PENDULUM_SIMULATOR_VE_OFF       0x28   /* fractional * 2^16 */
#define PENDULUM_SIMULATOR_FE_OFF       0x2c   /* fractional * 2^16 */

/* readbacks; latched when PH is read */
#define PENDULUM_SIMULATOR_DX_RB_OFF    0x30   /* see DX_OFF */
#define PENDULUM_SIMULATOR_PH_RB_OFF    0x34   /* see PH_OFF */
#define PENDULUM_SIMULATOR_T_RB_OFF     0x38   /* fractional * 2^8 */

#define PENDULUM_SIMULATOR_SIZE (0x400)

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
	uint64_t base;
	uint64_t size;
	int    (*read) (uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
	int    (*write)(uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
#ifdef __cplusplus
	udpsrv_range(
		uint32_t base,
		uint32_t size,
		int    (*read)(uint8_t *data, uint32_t nbytes, uint64_t off, int debug),
		int    (*write)(uint8_t *data, uint32_t nbytes, uint64_t off, int debug),
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

#ifdef __cplusplus
extern "C" {
#endif
	int streamIsRunning();
#ifdef __cplusplus
};
#endif

#endif

