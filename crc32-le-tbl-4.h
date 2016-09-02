 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CRC32_LE_TBL_4
#define CRC32_LE_TBL_4

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* (small) Table driven CRC32 */
uint32_t
crc32_le_t4(uint32_t crc, uint8_t *buf, unsigned len);

/* crc32(0, &{0xff,ff,ff,ff}, 4) ^ 0xffffffff */
#define CRC32_LE_POSTINVERT_GOOD 0x2144df1c

#ifdef __cplusplus
};
#endif

#endif
