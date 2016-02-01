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
