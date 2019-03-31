 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cpsw_aligned_mmio.h>

int
main()
{
uint64_t data[] = {
	0xa2fb283c4d0e9aec,
	0xbaca2029771d8292,
	0xf42414ba15995030,
	0xdc8d0666a492389c,
	0x7bf0b7c792cddf9a,
	0x390808595be41adc,
	0x1447c072d37eefdd
};

uint8_t  dbuf[sizeof(data)];
uint64_t tbuf[sizeof(data)];

unsigned i, j, o, l, k;
unsigned succ = 0;
uint8_t m1,mn;

	for ( i=0; i<=sizeof(data[0]); i++ ) {
		for ( j=0; j <= 32; j++ ) {
			memset(dbuf, 0, sizeof(dbuf));
			cpsw_read_aligned<uint64_t>( dbuf, ((uint8_t*)&data) + i, j );

			if ( memcmp(dbuf, ((uint8_t*)&data) + i, j) ) {
				printf("MISMATCH: i %d, j %d\n", i, j);
				return 1;
			} else {
				succ++;
			}
		}
	}

	printf("%d successful (read) comparisons\n", succ);
	if ( succ != i*j ) {
		printf("MISMATCH -- expected %d\n", i*j);
		return 1;
	}

	succ = 0;
	/* This test only works on a little-endian machine ! */
	for ( i=0; i<64; i++ ) {
		for ( j=i; j < 64 - i; j++ ) {
			for ( k=0; k<1; k++ ) {
				o  = (i/8);
				unsigned shift = i & 7;
				l  = (shift+j+7)/8;
				m1 =  (1<< shift) - 1;
				mn = ~((1<< ((shift+j) & 7)) - 1);
				if ( mn == 0xff )
					mn = 0;

				memcpy(tbuf, data, sizeof(data));
				cpsw_write_aligned<uint16_t>((uint8_t*)tbuf + o, (uint8_t*)&data[3] + o, l, m1, mn);

				uint64_t m = (((1ULL<<i) - 1) | ~((1ULL<<(i+j))-1));

				unsigned xx;
				for ( xx = 0; xx < k; xx++ ) {
					m = (m << 8) | 0xff;
				}

				uint64_t e = ( (data[0] & m) | (data[3] & ~m) );

				if ( tbuf[0] != e  ) {
					printf("MISMATCH (i %d, j %d, k %d, o %d, l %d) expected 0x%16llx, got 0x%16llx\n",
						i, j, k, o, l, (unsigned long long )e, (unsigned long long )tbuf[0]);
					printf("mn 0x%02x, m1 0x%02x\n", mn, m1);
					printf("mask    0x%16llx\n", (unsigned long long )m);
					printf("data[0] 0x%16llx\n", (unsigned long long )data[0]);
					printf("tbuf[0] 0x%16llx\n", (unsigned long long )tbuf[0]);
					printf("data[3] 0x%16llx\n", (unsigned long long )data[3]);
					return 1;
				} else {
					succ++;
				}

				
			}
		}
	}

	printf("%d successful writes\n", succ);
	return 0;
}
