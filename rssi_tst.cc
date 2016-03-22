#include <udpsrv_rssi_port.h>
#include <stdio.h>
#include <signal.h>


void sh(int sig)
{
	rssi_debug=1;
}

int
main()
{
unsigned j;

	if ( signal(SIGINT, sh) ) {
		perror("Unable to install signal handler");
	}

for ( j=0; j<1; j++ ) {
	RssiPort  server = CRssiPort::create(true);
	RssiPort  client = CRssiPort::create(false);
	ProtoPort sSink  = ISink::create("Server Sink");
	ProtoPort cSink  = ISink::create("Client Sink");
	int i = 0;

	LoopbackPorts loop = ILoopbackPorts::create(32,0,2);

	printf("Loopback created\n");
	client->attach( loop->getPortA() );
	server->attach( loop->getPortB() );

	sSink->attach( server );
	cSink->attach( client );

	server->start();
	client->start();
	cSink->start();
	sSink->start();

	for ( i=0; i<100000; i++ ) {
		BufChain bc = IBufChain::create();
		Buf b       = bc->createAtHead( IBuf::CAPA_ETH_HDR );
		memcpy(b->getPayload(), &i, sizeof(i));
		b->setSize( sizeof(i) );
		b.reset();
		sSink->push(bc, NULL);
	}

	server->dumpStats(stderr);
	client->dumpStats(stderr);

}
#if 0
	printf("Bufs Free %d, alloced %d in use %d\n",
		IBuf::numBufsFree(),
		IBuf::numBufsAlloced(),
		IBuf::numBufsInUse());
#endif
	return 0;
}
