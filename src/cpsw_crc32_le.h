 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_CRC32_LE_H
#define CPSW_CRC32_LE_H

#include <stdint.h>

struct CCpswCrc32LE {
public:
	static const uint32_t POLY = 0xedb88320;
private:
	static uint32_t *tbl();
public:

	uint32_t operator()(uint32_t crc_in, uint8_t *buf, unsigned long len);
};

#endif
