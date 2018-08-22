 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_crc32_le.h>

struct CpswCrc32Tbl {
public:
	const static unsigned LDTSZ = 8;

	uint32_t t[(1<<LDTSZ)];

	CpswCrc32Tbl()
	{
	unsigned i,j;
		for ( i = 0; i < sizeof(t)/sizeof(t[0]); i++ ) {
			uint32_t crc = i;
			for ( j = 0; j < LDTSZ; j++ ) {
				crc = (crc >> 1) ^ ( (crc & 1 ) ? CCpswCrc32LE::POLY : 0 );
			}
			t[i] = crc;
		}
	}
};

uint32_t *
CCpswCrc32LE::tbl()
{
static CpswCrc32Tbl t_;
	return t_.t;
}

uint32_t
CCpswCrc32LE::operator()(uint32_t crc, uint8_t *buf, unsigned long l)
{
uint8_t idx;
	while ( l-- ) {
		idx = ((uint8_t)crc) ^ *buf++;
		crc = ( crc >> 8 ) ^ tbl()[idx];
	}
	return crc;
}
