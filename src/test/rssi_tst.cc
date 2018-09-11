 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <udpsrv_rssi_port.h>
#include <stdio.h>
#include <signal.h>


static void sh(int sig)
{
	cpsw_rssi_debug=1;
}

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-s <sleep_us>] [-n <packets>] [-L <percent dropped packets>] [-G garblDepth] [-h]\n", nm);
	fprintf(stderr,"Note: garbled packet depth quickly leads to out-of sequence packets\n");
	fprintf(stderr,"      probability a packet is delayed more than N cycles is ((Depth-1)/Depth)^N\n");
}

int
main(int argc, char **argv)
{
unsigned j;
unsigned sleep_us                = 0;
unsigned dropped_packets_percent = 0;
unsigned garbl_depth             = 0;
unsigned n_packets               = 100000;
int      opt;
unsigned *i_p;
int      rval = 1;

	while ( (opt = getopt(argc, argv, "s:L:G:n:h")) > 0 ) {
		i_p = 0;
		switch (opt) {
			case 's': i_p = &sleep_us;                break;
			case 'L': i_p = &dropped_packets_percent; break;
			case 'G': i_p = &garbl_depth;             break;
			case 'n': i_p = &n_packets;               break;

			case 'h': rval = 0;
			default:
				usage(argv[0]);
				return rval;
		}
		if ( 1 != sscanf(optarg,"%i",i_p) ) {
			fprintf(stderr,"Unable to scan arg for option '%c'\n", opt);
			return 1;
		}
	}

	if ( dropped_packets_percent >= 100 ) {
		fprintf(stderr,"requested packet loss too high\n");
		usage(argv[0]);
		return 1;
	}

	if ( signal(SIGINT, sh) ) {
		perror("Unable to install signal handler");
	}

for ( j=0; j<1; j++ ) {
	RssiPort  server = CRssiPort::create(true);
	RssiPort  client = CRssiPort::create(false);
	ProtoPort sSink  = ISink::create("Server Sink");
	ProtoPort cSink  = ISink::create("Client Sink", sleep_us);
	unsigned i = 0;

	LoopbackPorts loop = ILoopbackPorts::create(32,dropped_packets_percent,garbl_depth);

	printf("Loopback created\n");
	client->attach( loop->getPortA() );
	server->attach( loop->getPortB() );

	sSink->attach( server );
	cSink->attach( client );

	server->start();
	client->start();
	cSink->start();
	sSink->start();

	for ( i=0; i<n_packets; i++ ) {
		BufChain bc = IBufChain::create();
		Buf b       = bc->createAtHead( IBuf::CAPA_ETH_HDR );
		memcpy(b->getPayload(), &i, sizeof(i));
		b->setSize( sizeof(i) );
		b.reset();
		sSink->push(bc, NULL);
	}

	server->dumpStats(stderr);
	client->dumpStats(stderr);

	sSink->stop();
	cSink->stop();
	client->stop();
	server->stop();


}
	printf("Left braces\n");
#if 0
	printf("Bufs Free %d, alloced %d in use %d\n",
		IBuf::numBufsFree(),
		IBuf::numBufsAlloced(),
		IBuf::numBufsInUse());
#endif
	return 0;
}
