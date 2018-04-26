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

#include <stdint.h>

/* Table for 0xedb88320 */
static uint32_t crc32_tbl_4[16] = {
        0x00000000,
        0x1db71064,
        0x3b6e20c8,
        0x26d930ac,
        0x76dc4190,
        0x6b6b51f4,
        0x4db26158,
        0x5005713c,
        0xedb88320,
        0xf00f9344,
        0xd6d6a3e8,
        0xcb61b38c,
        0x9b64c2b0,
        0x86d3d2d4,
        0xa00ae278,
        0xbdbdf21c
};


/* crc32(0, &{0xff,ff,ff,ff}, 4) ^ 0xffffffff */
static const uint32_t good = 0x2144df1c;

uint32_t
crc32_le_t4(uint32_t crc, uint8_t *buf, unsigned len)
{
	while (len--) {
		crc = crc ^ *buf;
		crc = (crc >> 4) ^ crc32_tbl_4[ crc & 0xf ];
		crc = (crc >> 4) ^ crc32_tbl_4[ crc & 0xf ];
		buf++;
	}
	return crc;
}

