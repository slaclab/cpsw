#include <cpsw_api_builder.h>
#include <cpsw_proto_mod_depack.h>

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define NGOOD 200

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-h] [-d <output queue depth>] [-L <log2(frameWinSize)>] [-l <fragWinSize>] [-T <timeout_us>]\n", nm);
}
int
main(int argc, char **argv)
{
uint8_t  buf[100000];
int64_t  got;
int      i;
CAxisFrameHeader hdr;
int      quiet = 1;
int      fram, lfram, errs, goodf;
int      attempts;
unsigned*i_p;

	unsigned qDepth = 12;
	unsigned ldFrameWinSize = 5;
	unsigned ldFragWinSize  = 5;
	unsigned timeoutUs = 10000000;

	while ( (i=getopt(argc, argv, "d:l:L:hT:")) > 0 ) {
		i_p = 0;
		switch ( i ) {
			case 'd': i_p = &qDepth;         break;
			case 'L': i_p = &ldFrameWinSize; break;
			case 'l': i_p = &ldFragWinSize;  break;
			case 'T': i_p = &timeoutUs;      break;
			case 'h': usage(argv[0]); return 0;
		}
		if ( i_p && 1 != sscanf(optarg,"%i",i_p)) {
			fprintf(stderr, "Unable to scan arg to option '-%c'\n", i);
			goto bail;
		}
	}

	if ( ldFrameWinSize > 12 || ldFragWinSize > 12 ) {
		fprintf(stderr,"Window size seems too large -- you give the log2 of the size!\n");
		goto bail;
	}

	if ( qDepth > 5000 ) {
		fprintf(stderr,"Queue Depth seems too large...\n");
		goto bail;
	}

try {
//NoSsiDev root = INoSsiDev::create("udp", "192.168.2.10");
NoSsiDev root = INoSsiDev::create("udp", "127.0.0.1");
Field    data = IField::create("data");

	root->addAtStream( data, 8193, timeoutUs, qDepth, ldFrameWinSize, ldFragWinSize );

	Path   strmPath = root->findByName("data");

	Stream strm = IStream::create( strmPath );

	lfram    = -1;
	errs     = 0;
	goodf    = 0;
	attempts = 0;
	while ( goodf < NGOOD ) {

		if ( ++attempts > goodf + 100 ) {
			fprintf(stderr,"Too many attempts\n");
			goto bail;
		}

		got = strm->read( buf, sizeof(buf), CTimeout(8000000), 0 );
		printf("Read %"PRIu64" octets\n", got);
		if ( 0 == got ) {
			fprintf(stderr,"Read -- timeout. Is udpsrv running?\n");
			goto bail;
		}

		if ( ! hdr.parse(buf, sizeof(buf)) ) {
			printf("bad header!\n");
			continue;
		}

		if ( ! hdr.getTailEOF(buf + got - hdr.getTailSize()) ) {
			printf("no EOF tag!\n");
			continue;
		}
		
		fram = hdr.getFrameNo();
		printf("Frame # %4i\n", fram);

		if ( lfram >= 0 && lfram + 1 != fram ) {
			errs++;
		}


		if ( ! quiet ) {
			for ( i=0; i<got - (int)hdr.getSize() - (int)hdr.getTailSize(); i++ ) {
				printf("0x%02x ", buf[hdr.getSize() + i]);
			}
			printf("\n");
		}
		goodf++;
		lfram = fram;
	}

	strmPath->tail()->dump();

	if ( errs ) {
		fprintf(stderr,"%d frames missing (got %d)\n", errs, goodf);	
		goto bail;
	}


} catch ( CPSWError e ) {
	fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	printf("TEST PASSED: %d consecutive frames received\n", goodf);

	printf("# bufs allocated: %4d\n", IBuf::numBufsAlloced());
	printf("# bufs free     : %4d\n", IBuf::numBufsFree());
	printf("# bufs in use   : %4d\n", IBuf::numBufsInUse());

	if ( IBuf::numBufsInUse() != 0 ) {
		fprintf(stderr,"Buffers leaked !\n");
		goto bail;
	}
	
	return 0;
bail:
	fprintf(stderr,"TEST FAILED\n");
	fprintf(stderr,"Note: due to statistical nature *very rare* test failures may happen\n");
	return 1;
}
