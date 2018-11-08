 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <udpsrv_regdefs.h>
#include <stdio.h>
#include <string.h>

uint8_t mem[MEM_SIZE] = {0};

int streamIsRunning()
{
	return !! (mem[REGBASE + REG_STRM_OFF] & 1);
}

void range_io_debug(struct udpsrv_range *r, int rd, uint64_t off, uint32_t nbytes)
{
unsigned n;
	if ( rd )
		printf("Read from");
	else
		printf("Writing to");

	printf(" %" PRIx64 " (%" PRId64 "); %d bytes\n", off, off, nbytes);

	for ( n=0; n<nbytes; n++ ) {
		printf("%02x ",mem[off+n]);	
		if ( (n & 15) == 15 ) printf("\n");
	}
	if ( n & 15 )
		printf("\n");
}

static int memread(struct udpsrv_range *r, uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	memcpy(data, mem+off, nbytes);
#ifdef DEBUG
	if ( debug )
		range_io_debug(r, 1, off, nbytes);
#endif
	return 0;
}

static int memwrite(struct udpsrv_range *r, uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
int      i;

	if ( off + nbytes > REGBASE + REG_RO_OFF && off < REGBASE + REG_RO_OFF + REG_RO_SZ ) {
		/* disallow write; set status */
#ifdef DEBUG
		if ( debug )
			printf("Rejecting write to read-only region\n");
#endif
		return -1;
	} else {
		memcpy( mem + off, data, nbytes );
		if ( off <= REGBASE + REG_CLR_OFF && off + nbytes >= REGBASE + REG_CLR_OFF + 4 ) {
			if (    mem[REGBASE + REG_CLR_OFF]
					|| mem[REGBASE + REG_CLR_OFF + 1]
					|| mem[REGBASE + REG_CLR_OFF + 2]
					|| mem[REGBASE + REG_CLR_OFF + 3]
			   ) {
				memset( mem + REGBASE + REG_SCR_OFF, 0xff, REG_SCR_SZ );
				/* write array in LE; first and last word have special pattern */
				mem[REGBASE + REG_ARR_OFF + 0] = 0xaa;
				mem[REGBASE + REG_ARR_OFF + 1] = 0xbb;
				mem[REGBASE + REG_ARR_OFF + 2] = 0xcc;
				mem[REGBASE + REG_ARR_OFF + 3] = 0xdd;
				for ( i=4; i<REG_ARR_SZ-4; i+=2 ) {
					mem[REGBASE + REG_ARR_OFF + i + 0] = i>>(0+1); // i.e., i/2
					mem[REGBASE + REG_ARR_OFF + i + 1] = i>>(8+1);
				}
				mem[REGBASE + REG_ARR_OFF + i + 0] = 0xfa;
				mem[REGBASE + REG_ARR_OFF + i + 1] = 0xfb;
				mem[REGBASE + REG_ARR_OFF + i + 2] = 0xfc;
				mem[REGBASE + REG_ARR_OFF + i + 3] = 0xfd;
#ifdef DEBUG
				if ( debug )
					printf("Setting\n");
#endif
			} else {
				memset( mem + REGBASE + REG_SCR_OFF, 0x00, REG_SCR_SZ );
				/* write array in BE; first and last word have special pattern */
				mem[REGBASE + REG_ARR_OFF + 0] = 0xdd;
				mem[REGBASE + REG_ARR_OFF + 1] = 0xcc;
				mem[REGBASE + REG_ARR_OFF + 2] = 0xbb;
				mem[REGBASE + REG_ARR_OFF + 3] = 0xaa;
				for ( i=4; i<REG_ARR_SZ-4; i+=2 ) {
					mem[REGBASE + REG_ARR_OFF + i + 1] = i>>(0+1); // i.e., i/2
					mem[REGBASE + REG_ARR_OFF + i + 0] = i>>(8+1);
				}
				mem[REGBASE + REG_ARR_OFF + i + 0] = 0xfd;
				mem[REGBASE + REG_ARR_OFF + i + 1] = 0xfc;
				mem[REGBASE + REG_ARR_OFF + i + 2] = 0xfb;
				mem[REGBASE + REG_ARR_OFF + i + 3] = 0xfa;
#ifdef DEBUG
				if ( debug )
					printf("Clearing\n");
#endif
			}
			memset( mem + REGBASE + REG_CLR_OFF, 0xaa, 4 );
		}
	}

#ifdef DEBUG
	if ( debug )
		range_io_debug(r, 0, off, nbytes);
#endif

	return 0;
}

static void memini(struct udpsrv_range *r)
{
unsigned i;
char    *dst = (char*)&mem[0x800];
	sprintf(dst, "Hello");
	for ( i=0; i<16; i+=2 ) {
		mem[REGBASE+i/2]    = (i<<4)|(i+1);
		mem[REGBASE+15-i/2] = (i<<4)|(i+1);
	}
	mem[REGBASE + REG_STRM_OFF] = 0;
}


static struct udpsrv_range memhdl("MEMORY", MEM_ADDR, sizeof(mem), memread, memwrite, memini);
