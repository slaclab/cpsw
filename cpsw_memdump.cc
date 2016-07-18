#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
#include <boost/atomic.hpp>

#include <string.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <getopt.h>

#include <vector>

#include <pthread.h>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <signal.h>

#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

using boost::atomic;
using boost::memory_order_acquire;
using boost::memory_order_release;

using std::vector;

class TestFailed {};

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-a <ip_addr>[:<port>[:<stream_port>]]] [-mhRrs] [-V <version>] [-S <length>] [-n <shots>] [-p <period>] [-d tdest] [-D tdest] [-M tdest] [-T timeout]\n", nm);
	fprintf(stderr,"       -a <ip_addr>:  destination IP\n");
	fprintf(stderr,"       -V <version>:  SRP version (1..3)\n");
	fprintf(stderr,"       -b          :  base address\n");
	fprintf(stderr,"       -s          :  size (bytes)\n");
	fprintf(stderr,"       -R          :  use RSSI (SRP)\n");
	fprintf(stderr,"       -D <tdest>  :  use tdest demuxer (SRP)\n");
	fprintf(stderr,"       -T <us>     :  set SRP timeout\n");
	fprintf(stderr,"       -f          :  specify file name to dump, else to console\n");
	fprintf(stderr,"       -t          :  specify text file format (binary else)\n");
	fprintf(stderr,"       -h          :  print this message\n\n\n");
}

int
main(int argc, char **argv)
{
int         rval    = 0;
const char *ip_addr = "192.168.2.10";
bool        use_mem = false;
int        *i_p;
int         vers    = 2;
int         srpTo   = 0;
unsigned    port    = 8192;
unsigned    sport   = 8193;
char        cbuf[100];
const char *col1    = NULL;
bool        srpRssi = false;
int         tDestSRP  = -1;
uint64_t    size = 0;
uint64_t    baseAddr = 0;
bool        text_file = false;
const char *file_name = "";




	for ( int opt; (opt = getopt(argc, argv, "a:V:s:htRD:T:b:f:")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr = optarg;     break;
			case 'm': use_mem = true;       break;
			case 's': 
				sscanf(optarg, "%" SCNu64, &size );
				break;
			case 'V': i_p     = &vers;      break;
			case 'h': usage(argv[0]);      
				return 0;
			case 'R': srpRssi = true;       break;
			case 'D': i_p     = &tDestSRP;  break;
			case 'T': i_p     = &srpTo;     break;
			case 'b':
				sscanf(optarg, "%" SCNu64, &baseAddr );
				break;
			case 'f': file_name = optarg;   break;
			case 't': text_file = true;     break;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				usage(argv[0]);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( vers != 1 && vers != 2 && vers != 3 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1..3\n", vers);
		throw TestFailed();
	}

	if ( (col1 = strchr(ip_addr,':')) ) {
		unsigned len = col1 - ip_addr;
		if ( len >= sizeof(cbuf) ) {
			fprintf(stderr,"IP-address string too long\n");
			throw TestFailed();
		}
		strncpy(cbuf, ip_addr, len);
		cbuf[len]=0;
		if ( strchr(col1+1,':') ) {
			if ( 2 != sscanf(col1+1,"%d:%d", &port, &sport) ) {
				fprintf(stderr,"Unable to scan ip-address (+ 2 ports)\n");
				throw TestFailed();
			}
		} else {
			if ( 1 != sscanf(col1+1,"%d", &port) ) {
				fprintf(stderr,"Unable to scan ip-address (+ 1 port)\n");
				throw TestFailed();
			}
		}
		ip_addr = cbuf;
	}

// read in BLOCK_SIZE chunks
#define BLOCK_SIZE 1024*16

try {
NetIODev  root = INetIODev::create("fpga", ip_addr);
MMIODev   mmio = IMMIODev::create ("mmio", baseAddr + size);
Field     f    = IIntField::create("memory", 8, false, 0);
uint8_t u8[BLOCK_SIZE];
	mmio->addAtAddress( f, baseAddr, size );

	{
	INetIODev::PortBuilder bldr = INetIODev::createPortBuilder();
	INetIODev::ProtocolVersion protoVers;
		switch ( vers ) {
			default: throw TestFailed();
			case 1: protoVers = INetIODev::SRP_UDP_V1; break;
			case 2: protoVers = INetIODev::SRP_UDP_V2; break;
			case 3: protoVers = INetIODev::SRP_UDP_V3; break;
		}
		bldr->setSRPVersion              (             protoVers );
		bldr->setUdpPort                 (                  port );
		bldr->setSRPTimeoutUS            (                 90000 );
		bldr->setSRPRetryCount           (                     5 );
		bldr->setSRPMuxVirtualChannel    (                     0 );
		bldr->useRssi                    (               srpRssi );
		if ( tDestSRP >= 0 ) {
			bldr->setTDestMuxTDEST       (              tDestSRP );
		}
		root->addAtAddress( mmio, bldr );

	}

	Path       pre      = IPath::create( root );
	ScalVal_RO mem      = IScalVal_RO::create( pre->findByName("mmio/memory") );
	IndexRange rng(0, BLOCK_SIZE - 1);

	std::ofstream dumpfile;
	dumpfile.open( file_name );
	if ( dumpfile.fail() )
	{
		throw;
	}
	for ( int i = 0; i < ( size + sizeof(u8) - 1 )/sizeof( u8 ); i++, ++rng )
	{
		if ( rng.getTo() >= size ) 
		{
			rng.setTo( size - 1 );
		}
		mem->getVal( u8, BLOCK_SIZE, &rng );
		for (int j = 0 ; j < ( rng.getTo() - rng.getFrom() ) ; j++)
		{
			if ( text_file )
			{
				if( ( ( j % 8 ) == 0 ) )
				{
					dumpfile << std::hex << "\n0x" << std::setfill('0') << std::setw(8) << ( baseAddr + j + rng.getFrom() ) << ": ";
				}
				dumpfile << std::hex << "0x" << std::setfill('0') << std::setw(2) << static_cast<int>( u8[j] ) << " ";
			}
			else //binary file
			{
				dumpfile << u8[j];
			}
		}
	} 


	dumpfile.close();
	
} catch (CPSWError &e) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}
	return rval;
}
