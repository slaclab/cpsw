 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_ALIGNED_MMIO_H
#define CPSW_ALIGNED_MMIO_H

#include <stdint.h>
#include <string.h>

template <typename T>
uint64_t cpsw_read_aligned(uint8_t *dst, uint8_t *src, size_t  n)
{
union {
	T        t;
	uint8_t  c[sizeof(T)];
}           buf0,  bufN;
volatile T *wad0, *wadN;
uintptr_t   off0,  offN;

	if ( 0 == n )
		return n;

	off0 = ((uintptr_t)src) & (sizeof(T) - 1);
	wad0 = (volatile T*) (((uintptr_t)src) - off0);

	offN = ((uintptr_t)src + n) & (sizeof(T) - 1);
	wadN = (volatile T*) (((uintptr_t)src + n) - offN);

    if ( off0 ) {
		buf0.t = *wad0; /* must be aligned access */
		if ( offN ) {
			if ( wad0 == wadN ) {
				memcpy( dst, buf0.c + off0, offN - off0 + 1 );
                return n;
			} else {
				bufN.t = *wadN; /* must be aligned access */
				memcpy( dst + n - offN, bufN.c, offN   );
			}
		}
		memcpy( dst, buf0.c + off0, sizeof(T) - off0 );
        wad0++;
		dst += sizeof(T) - off0;
	} else if ( offN ) {
		bufN.t = *wadN;
		memcpy( dst + n - offN, bufN.c, offN   );
	}
	while ( wad0 < wadN ) {
		buf0.t = *wad0;
		memcpy( dst, &buf0.t, sizeof(buf0.t) );
		wad0++;
		dst += sizeof(buf0.t);
	}
	return n;
}

template <typename T>
uint64_t cpsw_write_aligned(uint8_t *dst, uint8_t *src, size_t  n, uint8_t msk1, uint8_t mskN)
{
union {
	T        t;
	uint8_t  c[sizeof(T)];
}           buf0,  bufN;
volatile T *wad0, *wadN;
uintptr_t   off0,  offN;

	if ( 0 == n )
		return n;

	off0 = ((uintptr_t)dst) & (sizeof(T) - 1);
	wad0 = (volatile T*) (((uintptr_t)dst) - off0);

	offN = ((uintptr_t)dst + n - 1) & (sizeof(T) - 1);
	wadN = (volatile T*) (((uintptr_t)dst + n - 1) - offN);

	/* 1st word; aligned access */
	buf0.t = *wad0;

	if ( n == 1 ) {
		msk1 |= mskN;
	}

	/* merge first octet */
	buf0.c[off0] = (buf0.c[off0] & msk1) | (src[0] & ~msk1);

	if ( wad0 == wadN ) {

		if ( n > 1 ) {
			/* merge last octet (in same word) */
			buf0.c[offN] = (buf0.c[offN] & mskN) | (src[offN - off0] & ~mskN);
			memcpy( buf0.c + off0 + 1, src + 1, n - 2 );
		}

		*wad0 = buf0.t;
		return n;
	}

	memcpy( buf0.c + off0 + 1, src + 1, sizeof(T) - off0 - 1 );

	/* write back */
	*wad0 = buf0.t;

	/* last word; aligned access. Does not overlap the first */
	bufN.t = *wadN;

	bufN.c[offN] = (bufN.c[offN] & mskN) | (src[n - 1] & ~mskN);
	memcpy( bufN.c, src + n - 1 - offN, offN );
	*wadN = bufN.t;

    wad0++;
	src += sizeof(T) - off0;

	while ( wad0 < wadN ) {
		memcpy( &buf0.t, src, sizeof(buf0.t) );
		*wad0 = buf0.t;
		wad0++;
		src += sizeof(buf0.t);
	}
	return n;
}

#endif
