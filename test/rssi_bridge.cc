#include <iostream>
#include <cpsw_error.h>
#include <udpsrv_util.h>
#include <udpsrv_rssi_port.h>
#include <cpsw_thread.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

#include <arpa/inet.h>

//#define RSSI_BR_DEBUG

class CMover : public CRunnable {
private:
	ProtoPort to_, from_;

	CMover(const CMover&);
	CMover &operator=(const CMover&);
protected:
	virtual void *threadBody();
public:
	CMover(const char *name, ProtoPort to, ProtoPort from);
};

void*
CMover::threadBody()
{
	while ( 1 ) {
		BufChain bc = from_->pop( NULL );
		if ( bc ) {
#ifdef RSSI_BR_DEBUG
			printf("%s: got %ld bytes\n", getName().c_str(), bc->getSize());
			bool st =
#endif
			          to_->push( bc, NULL );
#ifdef RSSI_BR_DEBUG
			printf("  push: %s\n", st ? "OK" : "FAIL");
#endif
		}
	}
	return NULL;
}

CMover::CMover(const char *name, ProtoPort to, ProtoPort from)
: CRunnable( name ),
  to_( to ),
  from_( from )
{
}

static void
usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-h] -a <dest_ip>:<dest_port [-p <tcp_srv_port>]\n", nm);
	fprintf(stderr,"       RSSI <-> TCP bridge\n");
	fprintf(stderr,"       -a <dest_ip>:<dest_port>: remote address where a RSSI server is listening.\n");
	fprintf(stderr,"       -p <tcp_srv_port>       : local TCP port where we are listening for a connection.\n");
	fprintf(stderr,"                                 Defaults to <dest_port>\n");
	fprintf(stderr,"       -h                      : This message\n");
}

int
main(int argc, char **argv)
{
char           peer_ip[128];
unsigned short peer_port   = 8192;
unsigned short my_port;
bool           my_port_set = false;
int            rval        = 1;
unsigned short *s_p;
const char     *col;

int opt;

	::strncpy( peer_ip, "127.0.0.1", sizeof(peer_ip) );

	while ( (opt = getopt(argc, argv, "hp:a:")) > 0 ) {
		s_p = NULL;
		switch (opt) {
			case 'h':
				rval = 0;
			default:
				usage( argv[0] );
				return rval;
			case 'p':
				my_port_set = true;
				s_p = &my_port;
				break;

			case 'a':
				if ( ! (col = ::strchr(optarg,':')) ) {
					fprintf(stderr,"Error: -a argument misformed; doesn't contain ':'\n");
					return rval;
				}
				if ( 1 != sscanf( col+1, "%hi", &peer_port ) || peer_port == 0 ) {
					fprintf(stderr,"Error: bad or Nul peer port in -a\n");
					return rval;
				}
				::strncpy( peer_ip, optarg, sizeof(peer_ip) );
				peer_ip[col-optarg] = 0;

				if ( INADDR_NONE == inet_addr( peer_ip ) ) {
					fprintf(stderr,"Error: invalid IP address in -a\n");
					return rval;
				}

				break;
		}
		if ( s_p && 1 != sscanf(optarg, "%hi", s_p ) ) {
			fprintf(stderr,"Error: Unable to scan arg to option -%c\n", opt);
			return rval;
		}
	}

	if ( ! my_port_set )
		my_port = peer_port;

try {

UdpPort   udpPrt  = IUdpPort::create( NULL, 0, 0, 0 );
RssiPort  rssiPrt = CRssiPort::create( false );
TcpPort   tcpPrt  = ITcpPort::create( NULL, my_port );

		rssiPrt->attach( udpPrt );

ProtoPort p;

		udpPrt->connect( peer_ip, peer_port );

		for ( p=rssiPrt; p; p=p->getUpstreamPort() )
			p->start();
		for ( p=tcpPrt; p; p=p->getUpstreamPort() )
			p->start();

		CMover rssiToTcp( "RSSI->TCP", tcpPrt, rssiPrt );

		rssiToTcp.threadStart();

		CMover tcpToRssi( "TCP->RSSI", rssiPrt, tcpPrt );

		tcpToRssi.threadStart();

		rssiToTcp.threadJoin();
		tcpToRssi.threadJoin();

} catch (CPSWError &e) {
	std::cerr <<  e.getInfo() << "\n";
	throw;
}

	return rval;
}
