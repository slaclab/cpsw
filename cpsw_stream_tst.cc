#include <cpsw_api_builder.h>
#include <cpsw_proto_mod_depack.h>
#include <crc32-le-tbl-4.h>

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#ifdef WITH_YAML
#include <cpsw_yaml.h>
#include <fstream>
#endif

#define NGOOD 200

#undef  DEBUG

class TestFailed {};

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-h] [-s <port>] [-q <input_queue_depth>] [-Q <output queue depth>] [-L <log2(frameWinSize)>] [-l <fragWinSize>] [-T <timeout_us>] [-e err_percent] [-n n_frames] [-R] [-y dump-yaml] [-Y load-yaml]\n", nm);
}

extern int rssi_debug;

static void sendMsg(Stream strm, uint8_t m)
{
CAxisFrameHeader hdr;
uint8_t          buf[1500];
uint32_t         crc;
unsigned         i, endi;

	hdr.insert(buf, sizeof(buf));
	buf[ (endi = hdr.getSize()) ] = m;
	crc = crc32_le_t4( -1, buf+endi, 100 ) ^ -1;
	endi += 100;
	for ( i=0; i<sizeof(crc); i++ ) {
		buf[endi+i] = crc & 0xff;
		crc >>= 8;
	}
	endi += i;
	hdr.iniTail( buf + endi );
	endi += hdr.getTailSize();
	strm->write( buf, endi );
}

class StrmRxFailed {};

int
main(int argc, char **argv)
{
uint8_t  buf[100000];
int64_t  got;
unsigned i;
int      opt;
CAxisFrameHeader hdr;
int      quiet = 1;
int      fram, lfram;
unsigned errs, goodf;
unsigned attempts;
unsigned*i_p;
unsigned ngood = NGOOD;
unsigned nUdpThreads = 4;
unsigned useRssi     = 0;
unsigned tDest       = -1;
unsigned sport       = 8193;
const char *dmp_yaml = 0;
const char *use_yaml = 0;

	unsigned iQDepth = 40;
	unsigned oQDepth =  5;
	unsigned ldFrameWinSize = 5;
	unsigned ldFragWinSize  = 5;
	unsigned timeoutUs = 8000000;
	unsigned err_percent = 0;

	rssi_debug=0;

	while ( (opt=getopt(argc, argv, "d:l:L:hT:e:n:Rs:t:y:Y:")) > 0 ) {
		i_p = 0;
		switch ( opt ) {
			case 'q': i_p = &iQDepth;        break;
			case 'Q': i_p = &oQDepth;        break;
			case 'L': i_p = &ldFrameWinSize; break;
			case 'l': i_p = &ldFragWinSize;  break;
			case 'T': i_p = &timeoutUs;      break;
			case 'e': i_p = &err_percent;    break;
			case 's': i_p = &sport;          break;
			case 'n': i_p = &ngood;          break;
			case 'R': useRssi = 1;           break;
			case 't': i_p = &tDest;          break;
#ifdef WITH_YAML
			case 'Y': use_yaml    = optarg;   break;
			case 'y': dmp_yaml    = optarg;   break;
#else
			case 'y':
			case 'Y': fprintf(stderr,"YAML support not compiled in\n");
				throw TestFailed();
#endif
			default:
			case 'h': usage(argv[0]); return 1;
		}
		if ( i_p && 1 != sscanf(optarg,"%i",i_p)) {
			fprintf(stderr, "Unable to scan arg to option '-%c'\n", opt);
			goto bail;
		}
	}

	if ( ldFrameWinSize > 12 || ldFragWinSize > 12 ) {
		fprintf(stderr,"Window size seems too large -- you give the log2 of the size!\n");
		goto bail;
	}

	if ( iQDepth > 5000 || oQDepth > 5000 ) {
		fprintf(stderr,"Queue Depth seems too large...\n");
		goto bail;
	}

	if ( err_percent > 100 ) {
		fprintf(stderr,"-e <err_percent> must be in 0..100\n");
		goto bail;
	}

try {
Hub      root;

	if ( use_yaml ) {
#ifdef WITH_YAML
		root = IHub::loadYamlFile( use_yaml, "root" );
#endif
	} else {
	NetIODev netio = INetIODev::create("udp", "127.0.0.1");
	Field    data  = IField::create("data");

		root = netio;

	INetIODev::PortBuilder bldr( INetIODev::createPortBuilder() );

		bldr->setSRPVersion          ( INetIODev::SRP_UDP_NONE );
		bldr->setUdpPort             (                   sport );
		bldr->setUdpOutQueueDepth    (                 iQDepth );
		bldr->setUdpNumRxThreads     (             nUdpThreads );
		bldr->setDepackOutQueueDepth (                 oQDepth );
		bldr->setDepackLdFrameWinSize(          ldFrameWinSize );
		bldr->setDepackLdFragWinSize (           ldFragWinSize );
		bldr->useRssi                (                 useRssi );
		if ( tDest < 256 )
			bldr->setTDestMuxTDEST   (                   tDest );

		netio->addAtAddress( data, bldr );
	}

	Path   strmPath = root->findByName("data");

	Stream strm = IStream::create( strmPath );

	lfram    = -1;
	errs     = 0;
	goodf    = 0;
	attempts = 0;

	sendMsg( strm, 1 );
	sendMsg( strm, 1 );

	try {

	while ( goodf < ngood ) {

		if ( ++attempts > goodf + (goodf*err_percent)/100 + 10 ) {
			fprintf(stderr,"Too many attempts\n");
			throw StrmRxFailed();
		}

		if ( (attempts & 31) == 0 ) {
			// once in a while try to write something...
			// udpsrv will jam the CRC if they receive
			// corrupted data;	
			sendMsg( strm, 1 );
		}

		got = strm->read( buf, sizeof(buf), CTimeout(timeoutUs), 0 );

#ifdef DEBUG
		printf("Read %"PRIu64" octets\n", got);
#endif
		if ( 0 == got ) {
			fprintf(stderr,"Read -- timeout. Is udpsrv running?\n");
			throw StrmRxFailed();
		}

		// udpsrv computes CRC over payload only (excluding stream header + tail byte)
		if ( CRC32_LE_POSTINVERT_GOOD ^ -1 ^ crc32_le_t4(-1, buf + 8, got - 9) ) {
			fprintf(stderr,"Read -- frame with bad CRC (jammed write message?)\n");
			throw StrmRxFailed();
		}

		// udpsrv computes CRC over payload only (excluding stream header + tail byte)
		if ( CRC32_LE_POSTINVERT_GOOD ^ -1 ^ crc32_le_t4(-1, buf + 8, got - 9) ) {
			fprintf(stderr,"Read -- frame with bad CRC (jammed write message?)\n");
			throw StrmRxFailed();
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
#ifdef DEBUG
		printf("Frame # %4i\n", fram);
#endif

		if ( lfram >= 0 && lfram + 1 != fram ) {
			errs++;
		}


		if ( ! quiet ) {
			for ( i=0; i<got - hdr.getSize() - hdr.getTailSize(); i++ ) {
				printf("0x%02x ", buf[hdr.getSize() + i]);
			}
			printf("\n");
		}
		goodf++;
		lfram = fram;
	}

	} catch ( StrmRxFailed ) {
		sendMsg( strm, 0 );
		sendMsg( strm, 0 );
		goto bail;
	}
	sendMsg( strm, 0 );
	sendMsg( strm, 0 );

	strmPath->tail()->dump();

	if ( errs ) {
		fprintf(stderr,"%d frames missing (got %d)\n", errs, goodf);	
		if ( errs*100 > err_percent * ngood )
			goto bail;
	}

#ifdef WITH_YAML
	if ( dmp_yaml ) {
		CYamlFieldFactoryBase::dumpYamlFile( root, dmp_yaml, "root" );
	}
#endif


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
