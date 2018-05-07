 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <cpsw_proto_depack.h>

void
CPackHeaderUtil::setNum(uint8_t *p, unsigned bit_offset, unsigned bit_size, uint64_t val)
{
int      i,l;
unsigned shift = bit_offset % 8;
uint8_t  msk1, mskn;

	if ( bit_size == 0 )
		return;

	p += bit_offset/8;

	// mask of bits to preserve in the first and last word, respectively.

	l = (shift + bit_size + 7)/8;

	// l is > 0

	msk1 = (1 << shift) - 1;
	mskn = ~ ( (1 << ((shift + bit_size) % 8)) - 1 );
	if ( mskn == 0xff )
		mskn = 0x00;

	if ( 1 == l ) {
		msk1 |= mskn;
	}

	val <<= shift;

	// protocol uses little-endian representation

	// l cannot be 0 here
	p[0] = (val & ~msk1) | (p[0] & msk1);
	val >>= 8;

	for ( i = 1; i < l-1; i++ ) {
		p[i] = val;
		val >>= 8;
	}

	if ( i < l ) {
		p[i] = (val & ~mskn) | (p[i] & mskn);
	}
}

uint64_t
CPackHeaderUtil::getNum(uint8_t *p, unsigned bit_offset, unsigned bit_size)
{
int      i;
unsigned shift = bit_offset % 8;
unsigned rval;

	if ( bit_size == 0 )
		return 0;

	p += bit_offset/8;

	// protocol uses little-endian representation
	rval = 0;
	for ( i = (shift + bit_size + 7)/8 - 1; i>=0; i-- ) {
		rval = (rval << 8) + p[i];
	}

	rval >>= shift;

	rval &= (1<<bit_size)-1;

	return rval;
}
