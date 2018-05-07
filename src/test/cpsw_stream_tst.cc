 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_proto_mod_depack.h>
#include <crc32-le-tbl-4.h>
#include <pthread.h>

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define NGOOD 200

#undef  DEBUG

class TestFailed {};

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-h] [-s <port>] [-q <input_queue_depth>] [-Q <output queue depth>] [-L <log2(frameWinSize)>] [-l <fragWinSize>] [-T <timeout_us>] [-e err_percent] [-n n_frames] [-R] [-y dump-yaml] [-Y load-yaml] [-2]\n", nm);
}

#define STRT(chnl) (0x01<<(chnl))
#define STOP(chnl) (0x10<<(chnl))

extern int rssi_debug;

class StrmRxFailed {};

#define NUM_STREAMS 2

struct StrmCtxt {
	int       depack2;
	unsigned  ngood;
	unsigned  timeoutUs;
	unsigned  err_percent;
	unsigned  tdest;
	int       quiet;
	Path      strmPath;
	unsigned  goodf;
	int       status;  // written by thread
	bool      started; // written by main
	pthread_t tid;
	int       chnl;
};

template <typename T>
unsigned getHdrSize(uint8_t *buf, unsigned len)
{
T hdr;
	hdr.insert(buf, len);
	return hdr.getSize();
}

unsigned
getFrameNo(unsigned hsiz, uint8_t *buf)
{
uint32_t v = 0;
int      i;
	/* transmitted as part of the payload by udpsrv */
	for ( i = sizeof(v) - 1; i>=0; --i ) {
		v = (v<<8) | buf[ hsiz + 1 + i ];
	}
	return v;
}

template <typename T>
int parseHdr(uint8_t *buf, unsigned len, int *fram,  unsigned *hsiz, unsigned *tsiz, unsigned *tdest)
{
T   hdr;
	if ( ! hdr.parse(buf, len) ) {
		printf("bad header!\n");
		return -1;
	}

	if ( ! hdr.parseTailEOF(buf + len - hdr.getTailSize()) ) {
		printf("no EOF tag!\n");
		return -1;
	}
		
	*fram  = getFrameNo(hdr.getSize(), buf);
	*hsiz  = hdr.getSize();
	*tsiz  = hdr.getTailSize();
	*tdest = hdr.getTDest();
	return 0;
}


static void sendMsg(Stream strm, uint8_t m, int depack2)
{
uint8_t          buf[1500];
uint32_t         crc;
unsigned         i, endi;

	if ( depack2 ) {
		endi = getHdrSize<CDepack2Header>(buf, sizeof(buf));
	} else {
		endi = getHdrSize<CAxisFrameHeader>(buf, sizeof(buf));
	}

	buf[ endi ] = m;
	for (i = 1; i<100; i++)
		buf[endi+i] = i;
	crc = crc32_le_t4( -1, buf+endi, 100 ) ^ -1;
	endi += 100;
	for ( i=0; i<sizeof(crc); i++ ) {
		buf[endi+i] = crc & 0xff;
		crc >>= 8;
	}
	endi += i;
	if ( depack2 ) {
		/* must provide the tail */
		endi += CDepack2Header::appendTail(buf, endi, true);
		crc   = crc32_le_t4( -1,  buf, endi - sizeof(crc) );
		CDepack2Header::insertCrc( buf + endi - CDepack2Header::getTailSize(), ~crc );
	}
	strm->write( buf, endi );
}

void *
strmRcvr(void *arg)
{
StrmCtxt *c = (StrmCtxt *)arg;

uint8_t  buf[100000];
int      fram, lfram;
unsigned errs;
unsigned attempts;
unsigned hsiz, tsiz;
int64_t  got;
unsigned tdest;

	lfram    = -1;
	errs     = 0;
	c->goodf = 0;
	attempts = 0;

try {
printf("Rcvr startup: %s\n", c->strmPath ? c->strmPath->toString().c_str() : "<NIL>");

	Stream strm = IStream::create( c->strmPath );
	sendMsg( strm, STRT(c->chnl) , c->depack2 );

	try {


	while ( c->goodf < c->ngood ) {

		if ( ++attempts > c->goodf + (c->goodf*c->err_percent)/100 + 10 ) {
			fprintf(stderr,"Too many attempts\n");
			throw StrmRxFailed();
		}

		if ( (attempts & 31) == 0 ) {
			// once in a while try to write something...
			// udpsrv will jam the CRC if they receive
			// corrupted data;	
			sendMsg( strm, STRT(c->chnl), c->depack2 );
		}

		got = strm->read( buf, sizeof(buf), CTimeout(c->timeoutUs), 0 );

#ifdef DEBUG
		printf("Read %"PRIu64" octets\n", got);
#endif
		if ( 0 == got ) {
			fprintf(stderr,"Read -- timeout. Is udpsrv running?\n");
			throw StrmRxFailed();
		}

#ifdef DEBUG
		for ( unsigned k = 0; k<8; k++ )
			printf("FRM/FRG 0x%"PRIx8"\n", buf[k]);
#endif

		if ( c->depack2 ) {
			if ( parseHdr<CDepack2Header>(buf, got, &fram, &hsiz, &tsiz, &tdest) )
				continue;
		} else {
			if ( parseHdr<CAxisFrameHeader>(buf, got, &fram, &hsiz, &tsiz, &tdest) )
				continue;
		}

		if ( c->tdest != (unsigned)-1 && c->tdest != tdest ) {
			fprintf(stderr,"TDEST Mismatch: got 0x%02x -- expected 0x%02x\n", tdest, c->tdest);
			throw StrmRxFailed();
		}

		// udpsrv computes CRC over payload only (excluding stream header + tail byte)
		if ( CRC32_LE_POSTINVERT_GOOD ^ -1 ^ crc32_le_t4(-1, buf + hsiz, got - (hsiz + tsiz)) ) {
			fprintf(stderr,"Read -- frame with bad CRC (jammed write message?)\n");
			throw StrmRxFailed();
		}

#ifdef DEBUG
		printf("Frame # %4i @TDEST 0x%02x\n", fram, tdest);
#endif

		if ( ! c->depack2 && lfram >= 0 && lfram + 1 != fram ) {
			errs++;
		}

		if ( ! c->quiet ) {
			int i;
			for ( i=0; i<got - hsiz - tsiz; i++ ) {
				printf("0x%02x ", buf[hsiz + i]);
			}
			printf("\n");
		}
		c->goodf++;
		lfram = fram;
	}

	} catch ( StrmRxFailed ) {
		sendMsg( strm, STOP(c->chnl), c->depack2 );
		sendMsg( strm, STOP(c->chnl), c->depack2 );
		goto bail;
	}

	sendMsg( strm, STOP(c->chnl), c->depack2 );
	sendMsg( strm, STOP(c->chnl), c->depack2 );

} catch ( CPSWError e ) {
		fprintf(stderr,"CPSW Error in reader thread (%s): %s\n", c->strmPath->toString().c_str(), e.getInfo().c_str());
		throw;
}

	if ( errs ) {
		fprintf(stderr,"%d frames missing (got %d)\n", errs, c->goodf);	
		if ( errs*100 > c->err_percent * c->ngood )
			goto bail;
	}

	c->status = 0;

bail:
	return 0;
}


int
main(int argc, char **argv)
{
int      depack2     = 0;
unsigned ngood       = NGOOD;
int      quiet       = 1;
unsigned nUdpThreads = 4;
unsigned useRssi     = 0;
unsigned tDest       = 0;
unsigned sport       = 8193;
const char *dmp_yaml = 0;
const char *use_yaml = 0;
unsigned err_percent = 0;
int      err;
int      i;
int      opt;
unsigned*i_p;
unsigned goodf;
StrmCtxt ctxt[NUM_STREAMS];

	unsigned iQDepth = 40;
	unsigned oQDepth =  5;
	unsigned ldFrameWinSize = 5;
	unsigned ldFragWinSize  = 5;
	unsigned timeoutUs = 8000000;

	rssi_debug=0;

	for ( i=0; i<NUM_STREAMS; i++ ) {
		ctxt[i].status  = -1;
		ctxt[i].started = false;
		ctxt[i].chnl    = i;
		ctxt[i].tdest   = -1;
	}

	while ( (opt=getopt(argc, argv, "d:l:L:hT:e:n:Rs:t:y:Y:2")) > 0 ) {
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
			case 'Y': use_yaml    = optarg;  break;
			case 'y': dmp_yaml    = optarg;  break;
			case '2': depack2 = 1;           break;
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
		root = IPath::loadYamlFile( use_yaml, "root" )->origin();
	} else {
	NetIODev netio = INetIODev::create("udp", "127.0.0.1");
	Field    data  = IField::create("data");
	Field    data1 = IField::create("data1");

		root = netio;

	ProtoStackBuilder bldr( IProtoStackBuilder::create() );

	if ( depack2 ) {
		sport   = 8204;
		useRssi = 1;
	}

		bldr->setSRPVersion          ( IProtoStackBuilder::SRP_UDP_NONE );
		bldr->setUdpPort             (                            sport );
		bldr->setUdpOutQueueDepth    (                          iQDepth );
		bldr->setUdpNumRxThreads     (                      nUdpThreads );
	if ( depack2 ) {
		bldr->setDepackVersion       ( IProtoStackBuilder::DEPACKETIZER_V2 );
	}
		bldr->setDepackOutQueueDepth (                          oQDepth );
		bldr->setDepackLdFrameWinSize(                   ldFrameWinSize );
		bldr->setDepackLdFragWinSize (                    ldFragWinSize );
		bldr->useRssi                (                          useRssi );
		if ( depack2 && tDest > 254 ) {
			tDest = 0;
		}
		ctxt[0].tdest = tDest;
		if ( tDest < 256 )
			bldr->setTDestMuxTDEST   (                            tDest );

		netio->addAtAddress( data, bldr );

		ctxt[1].tdest = (tDest + 2) & 255;
		bldr->setTDestMuxTDEST( ctxt[1].tdest );
		netio->addAtAddress( data1, bldr );
	}

	if ( dmp_yaml ) {
		IYamlSupport::dumpYamlFile( root, dmp_yaml, "root" );
	}


	for ( i=0; i<NUM_STREAMS; i++ ) {
		ctxt[i].depack2       = depack2;
		ctxt[i].ngood         = ngood;
		ctxt[i].timeoutUs     = timeoutUs;
		ctxt[i].err_percent   = err_percent;
		ctxt[i].quiet         = quiet;
	}
	ctxt[0].strmPath  = root->findByName("data");
	ctxt[1].strmPath  = root->findByName("data1");

	for ( i=0; i < (depack2 ? NUM_STREAMS : 1); i++ ) {

		if ( (err = pthread_create( &ctxt[i].tid, 0, strmRcvr, &ctxt[i] )) ) {
			fprintf(stderr,"Unable to create thread: %s\n", strerror(err));
			exit(1);
		} else {
			ctxt[i].started = true;
		}

	}

	for ( i=0; i<NUM_STREAMS; i++ ) {
		if ( ! ctxt[i].started ) {
			continue;
		}
		if ( (err = pthread_join( ctxt[i].tid , 0 )) ) {
			fprintf(stderr,"Unable to join thread: %s\n", strerror(err));
		}
	}

	if ( ctxt[0].strmPath )
		ctxt[0].strmPath->tail()->dump();

	for ( i=0; i<NUM_STREAMS; i++ ) {
		ctxt[i].strmPath.reset();
	}

} catch ( CPSWError e ) {
	fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	goodf = 0;
	for ( i=0; i<NUM_STREAMS; i++ ) {
		if ( ! ctxt[i].started )
			continue;
		if ( ctxt[i].status ) {
			goto bail;
		}
		goodf += ctxt[i].goodf;
	}

	printf("TEST PASSED: %d %sframes received\n", goodf, depack2 ? "" : "consecutive ");

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
