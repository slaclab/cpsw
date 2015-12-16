#include <cpsw_api_builder.h>
#include <cpsw_proto_mod_depack.h>

#include <stdio.h>
#include <inttypes.h>

#ifndef PRIu64
#define PRIu64 "lu"
#endif

int
main(int argc, char **argv)
{
//NoSsiDev root = INoSsiDev::create("udp", "192.168.2.10");
NoSsiDev root = INoSsiDev::create("udp", "127.0.0.1");
Field    sink = IField::create("sink");
uint8_t  buf[100000];
int64_t  got;
int      i;
CAxisFrameHeader hdr;
int      quiet = 1;

	unsigned qDepth = 12;
	unsigned ldFrameWinSize = 4;
	unsigned ldFragWinSize  = 4;

	root->addAtStream( sink, 8193, 25000000, qDepth, ldFrameWinSize, ldFragWinSize );

	Stream strm = IStream::create( root->findByName("sink") );

	while ( 1 ) {
		got = strm->read( buf, sizeof(buf), CTimeout(8000000), 0 );
		printf("Read %"PRIu64" octets\n", got);

		if ( ! hdr.parse(buf, sizeof(buf)) ) {
			printf("bad header!\n");
			continue;
		}

		if ( ! hdr.getTailEOF(buf + got - hdr.getTailSize()) ) {
			printf("no EOF tag!\n");
			continue;
		}
		
		printf("Frame # %4i\n", hdr.getFrameNo());

		if ( ! quiet ) {
			for ( i=0; i<got - (int)hdr.getSize() - (int)hdr.getTailSize(); i++ ) {
				printf("0x%02x ", buf[hdr.getSize() + i]);
			}
			printf("\n");
		}
	}

	

	
}
