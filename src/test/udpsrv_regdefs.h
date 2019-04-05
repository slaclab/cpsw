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

#include <stdio.h>

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

#define MEM_END (MEM_ADDR + MEM_SIZE) /* 0x00100000 */

#define AXI_SPI_EEPROM_ADDR           MEM_END /* 0x00100000 */
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

#define PENDULUM_SIMULATOR_SIZE        0x400

#define PENDULUM_SIMULATOR_TDEST          44

#define DAQMUX_ADDR                     (MEM_END + 0x1000)  /* 0x0101000 */
#define DAQMUX_SIZE                     0x1000

#define BSA_ADDR                        (DAQMUX_ADDR + DAQMUX_SIZE)         /* 0x0102000 */
#define BSA_SIZE                        0x1000
#define BSA_START_ADDR_OFF              0x0000
#define BSA_START_ADDR_SIZE             (4*sizeof(uint64_t))
#define BSA_END_ADDR_OFF                0x0200
#define BSA_END_ADDR_SIZE               (4*sizeof(uint64_t))

#define DAC_TABLE_0_ADDR                (MEM_END + 0x10000) /* 0x00110000 */
#define DAC_TABLE_SIZE                  (0x10000)
#define DAC_TABLE_1_ADDR                (DAC_TABLE_0_ADDR + DAC_TABLE_SIZE) /* 0x01200000 */
#define DAC_TABLE_2_ADDR                (DAC_TABLE_1_ADDR + DAC_TABLE_SIZE) /* 0x01300000 */
#define DAC_TABLE_3_ADDR                (DAC_TABLE_2_ADDR + DAC_TABLE_SIZE) /* 0x01400000 */
#define DAC_TABLE_TBL_OFF               (0x00000)
#define DAC_TABLE_TBL_SIZE              (0x1000*4)

#define GENERICADC0_TDEST                50
#define GENERICADC1_TDEST                51
#define GENERICADC2_TDEST                52
#define GENERICADC3_TDEST                53

#define DEBUG

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

struct udpsrv_range;


#define RANGE_VIOLATION(off,siz,regoff,regsiz) \
	( (off) < (regoff) || ( (off) + (siz) > (regoff) + (regsiz)) )

extern
#ifdef __cplusplus
"C"
#endif
struct udpsrv_range *udpsrv_ranges;

inline int hostIsLE()
{
union { uint16_t s; uint8_t c[2]; } u;
	u.s = 1;
	return u.c[0];
}

struct udpsrv_range {
	struct udpsrv_range *next;
	uint64_t             base;
	uint64_t             size;
	const char          *name;
	int    (*read) (struct udpsrv_range *self, uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
	int    (*write)(struct udpsrv_range *self, uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
#ifdef __cplusplus
	udpsrv_range(
		const char *name,
		uint64_t    base,
		uint64_t    size,
		int       (*read)(struct udpsrv_range *self, uint8_t *data, uint32_t nbytes, uint64_t off, int debug),
		int       (*write)(struct udpsrv_range *self, uint8_t *data, uint32_t nbytes, uint64_t off, int debug),
		void      (*init)(struct udpsrv_range *self)
	)
	:next(udpsrv_ranges), base(base), size(size), name(name), read(read), write(write)
	{
printf("Registering range %" PRIx64 " %" PRIx64 "(%s)\n", base, size, name);
		udpsrv_ranges = this;
		if ( init )
			init( this );
	}
#endif
};

#ifdef __cplusplus
extern "C" {
#endif
	int streamIsRunning();

#define STREAMBUF_HEADROOM 8
#define STREAMBUF_TAILROOM 16

	/* send a stream message; the buffer must have 8-bytes at
	 * the head reserved for the packet header and extra space
	 * at the tail. The max. available space is 'maxsize' the
	 * message size (incl. header) 'size'.
	 */
	void streamSend(uint8_t *buf, int maxsize, int size, uint8_t tdest);


	void range_io_debug(struct udpsrv_range *r, int rd, uint64_t off, uint32_t nbytes);

	int  register_io_range_1(const char *sysfs_name_and_off);
	int  register_io_range  (const char *sysfs_name, uint64_t start_addr);

#ifdef __cplusplus
};
#endif

#endif

