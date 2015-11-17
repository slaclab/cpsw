#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_hub.h>

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <getopt.h>

#include <sys/time.h>
#include <sys/resource.h>


#define VLEN 123
#define ADCL 10

#ifndef PRIx64
#define PRIx64 "lx"
#endif
#ifndef PRIx32
#define PRIx32 "x"
#endif

class TestFailed {};

#define NELMS 100000

static uint64_t tv2usec(struct timeval *a)
{
return (uint64_t)a->tv_sec * 1000000ULL + (uint64_t)a->tv_usec;
}

static uint64_t us_so_far()
{
struct rusage r;
	if ( getrusage( RUSAGE_SELF, &r ) ) {
		perror("getrusage");
		throw TestFailed();
	}
	return tv2usec( &r.ru_utime ) + tv2usec( &r.ru_stime );
}

int
main(int argc, char **argv)
{
char nm[100];
uint64_t init_us  = us_so_far();
uint64_t build_us = 0;
uint64_t lkup_us  = 0;
uint64_t read_us  = 0;

int  elsz = 32;
int  elszb;
int  *i_p;
int  lsb  = 0;
int  nelms = NELMS;

bool is_signed = true;

	for ( int opt; (opt=getopt(argc, argv, "s:S:n:")) > 0; ) {
		i_p = 0;
		switch (opt) {
			case 's': i_p = &elsz;  break;
			case 'S': i_p = &lsb;   break;
			case 'n': i_p = &nelms; break;
			default:
				fprintf(stderr,"Unknown option '-%c' -- usage %s [-s element_size_in_bits] [-S lsb_shift] [-n nelms]\n", opt, argv[0]);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg,"%i",i_p) ) {
			fprintf(stderr,"Unable to scan integer arg to '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( elsz < 0 || elsz > 64 ) {
		fprintf(stderr,"Invalid element size '%i' -- must be 0 <= elsz <= 64\n", elsz);
		throw TestFailed();
	}

	if ( lsb < 0 || lsb > 7 ) {
		fprintf(stderr,"Invalid lsb shift '%i' -- must be 0 <= lsb_shift < 7\n", elsz);
		throw TestFailed();
	}

	if ( lsb + elsz > 64 ) {
		fprintf(stderr,"shift + element size must be <= 64 for this test\n");
		throw TestFailed();
	}

	if ( nelms <= 0 ) {
		fprintf(stderr,"nelms must be > 0\n");
		throw TestFailed();
	}

	elszb = (elsz + lsb + 7)/8;

try {

MMIODev   mmio = IMMIODev::create ("mmio", elszb*nelms, BE);
MemDev    rmem = IMemDev::create  ("rmem", mmio->getSize());
ScalVal_RO vals[nelms];
uint64_t   uv;

	rmem->addAtAddress( mmio );

	uint8_t *bufp = rmem->getBufp();

	for (int i=0; i<nelms; i++) {
		::sprintf(nm,"r-%i",i);
		mmio->addAtAddress( IIntField::create(nm, elsz, is_signed, lsb), 0x00+elszb*i );
		int j;
		bufp += elszb;
		uv = (uint64_t)i;
		uv <<= lsb;
		for ( j=1; j<=elszb; j++ ) {
			bufp[-j] = uv;
			uv >>= 8;
		}
	}

	build_us = us_so_far() - init_us;

	Path pre = IPath::create( rmem );

	for (int i=0; i<nelms; i++) {
		::sprintf(nm,"mmio/r-%i",i);
		vals[i] = IScalVal_RO::create( pre->findByName(nm) );
	}

	lkup_us = us_so_far() - build_us;

	uint32_t msk = elsz >= 32 ? 0xffffffff : (1<<elsz) - 1;
	uint32_t sgn = elsz >= 32 || !is_signed ? 0 : (1<<(elsz-1)); 

	for (int i=0; i<nelms; i++) {
		uint32_t v;
		uint32_t e = (uint32_t)i & msk;
		if ( e & sgn )
			e |= ~msk;
		vals[i]->getVal( &v, 1 );
		if ( v != e ) {
			printf( "FAILED: got 0x%08"PRIx32" -- expected 0x%08"PRIx32"\n", v, e );
			throw TestFailed();
		}
	}

	read_us = us_so_far() - lkup_us;
} catch (CPSWError &e) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	printf("Large test successful; read %i registers of size %u-bits\n", nelms, (unsigned)elsz);
	printf("Building hierarchy:                 %8"PRIu64"us\n", build_us);
	printf("Name lookup:                        %8"PRIu64"us\n", lkup_us);
	printf("Reading (from memory pseudo device) %8"PRIu64"us\n", read_us);

	if ( cpsw_obj_count != 1 ) {
		fprintf(stderr,"FAILED -- %i Objects leaked!\n", cpsw_obj_count - 1);
		throw TestFailed();
	}

	return 0;
}
