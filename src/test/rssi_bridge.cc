 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <iostream>
#include <cpsw_error.h>
#include <udpsrv_util.h>
#include <udpsrv_rssi_port.h>
#include <cpsw_thread.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <vector>
#include <stdint.h>

#include <arpa/inet.h>
#include <boost/make_shared.hpp>

#include <unistd.h>

using boost::make_shared;

#define RSSI_BR_DEBUG

class CMover : public CRunnable {
private:
	ProtoPort to_, from_;
	int       dbg_;

	CMover(const CMover&);
	CMover &operator=(const CMover&);
protected:
	virtual void *threadBody();
public:
	CMover(const char *name, ProtoPort to, ProtoPort from, int debug);

	virtual ~CMover();
};

typedef shared_ptr<CMover> Mover;

class Bridge : public ITcpConnHandler {
private:
	Mover     udpToTcp_;
	Mover     tcpToUdp_;
	ProtoPort udpTop_;
	ProtoPort tcpTop_;
	unsigned  flags_;

public:
	typedef enum { NONE = 0, RSSI=1, DEBUG=2, ALWAYSCONN=4 } Flags;

	Bridge(const char *peerIp, unsigned short peerPort, unsigned short myPort, Flags flags);


	bool useRssi()
	{
		return !!(flags_ & RSSI);
	}

	bool debug()
	{
		return !!(flags_ & DEBUG);
	}

	virtual void up();
	virtual void down();

	void shutdown();

	~Bridge();
};

void*
CMover::threadBody()
{
	while ( 1 ) {
		BufChain bc = from_->pop( NULL );
		if ( bc ) {
#ifdef RSSI_BR_DEBUG
			if ( dbg_ ) {
				printf("%s: got %ld bytes\n", getName().c_str(), bc->getSize());
			}
			bool st =
#endif
			          to_->push( bc, NULL );
#ifdef RSSI_BR_DEBUG
			if ( dbg_ ) {
				printf("  push: %s\n", st ? "OK" : "FAIL");
			}
#endif
		}
	}
	return NULL;
}

CMover::CMover(const char *name, ProtoPort to, ProtoPort from, int debug)
: CRunnable( name ),
  to_( to ),
  from_( from ),
  dbg_( debug )
{
}

CMover::~CMover()
{
	threadStop();
}

Bridge::Bridge(const char *peerIp, unsigned short peerPort, unsigned short myPort, Flags flags)
: flags_(flags)
{
TcpConnHandler connHdlr = (flags & Bridge::ALWAYSCONN) ? TcpConnHandler() : this;

UdpPort   udpPrt  = IUdpPort::create( NULL, 0, 0, 0 );
TcpPort   tcpPrt  = ITcpPort::create( NULL, myPort, connHdlr );

		if ( useRssi() ) {
			udpTop_ = CRssiPort::create( false );
			udpTop_->attach( udpPrt );
		} else {
			udpTop_ = udpPrt;
		}

		tcpTop_ = tcpPrt;

		udpPrt->connect( peerIp, peerPort );

		for ( ProtoPort p=tcpTop_; p; p=p->getUpstreamPort() )
			p->start();

		if ( ! connHdlr ) {
			up();
		}

		try {

			udpToTcp_ = make_shared<CMover>( "UDP->TCP", tcpTop_, udpTop_, debug() );
			udpToTcp_->threadStart();

			tcpToUdp_ = make_shared<CMover>( "TCP->UDP", udpTop_, tcpTop_, debug() );
			tcpToUdp_->threadStart();

		} catch (...) {
			shutdown();
			throw;
		}
};

// Implement a TCP connection handler so that we attempt to connect the 
// UDP side only when contacted from TCP. This way we can always start
// the bridge on a standard set of ports and leave it to the remote peer
// to talk to the ones where something is actually listening on the UDP
// side.

void
Bridge::up()
{
	// Start up UDP side
	for ( ProtoPort p=udpTop_; p; p=p->getUpstreamPort() )
		p->start();
}

void
Bridge::down()
{
	// Shut down UDP side
	for ( ProtoPort p=udpTop_; p; p=p->getUpstreamPort() )
		p->stop();
}

void
Bridge::shutdown()
{
	for ( ProtoPort p=udpTop_; p; p=p->getUpstreamPort() )
		p->stop();
	for ( ProtoPort p=tcpTop_; p; p=p->getUpstreamPort() )
		p->stop();
}

Bridge::~Bridge()
{
	tcpToUdp_->threadCancel();
	tcpToUdp_->threadJoin();
	udpToTcp_->threadCancel();
	udpToTcp_->threadJoin();
	shutdown();
}

static void
usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-hdC] -a <dest_ip> [-p <dst_port>[:<tcp_srv_port>]] [-u <dst_port>[:<tcp_srv_port>]]\n", nm);
	fprintf(stderr,"       RSSI <-> TCP bridge\n");
	fprintf(stderr,"       -a <dest_ip>              : remote address where a RSSI server is listening.\n");
	fprintf(stderr,"       -p <dst_port>[:<tcp_port>]: <dst_port> identifies the remote UDP port and <tcp_port>\n");
	fprintf(stderr,"                                   the local TCP port where this program is listening. It\n");
	fprintf(stderr,"                                   defaults to <dst_port>.\n");
	fprintf(stderr,"                                   Note that multiple -p options may be given in order to\n");
	fprintf(stderr,"                                   bridge multiple connections.\n");
	fprintf(stderr,"       -u <dst_port>[:<tcp_port>]: bridge pure UDP connection **without RSSI**\n");
	fprintf(stderr,"                                   <dst_port> identifies the remote UDP port and <tcp_port>\n");
	fprintf(stderr,"                                   the local TCP port where this program is listening. It\n");
	fprintf(stderr,"                                   defaults to <dst_port>.\n");
	fprintf(stderr,"                                   Note that multiple -u options may be given in order to\n");
	fprintf(stderr,"                                   bridge multiple connections.\n");
	fprintf(stderr,"       -h                        : This message.\n");
#ifdef RSSI_BR_DEBUG
	fprintf(stderr,"       -d                        : Enable debug messages\n");
#else
	fprintf(stderr,"       -d                        : Debug support not compiled\n");
#endif
	fprintf(stderr,"       -C                        : Always connect UDP side. If this is omitted\n");
	fprintf(stderr,"                                   then connections on the UDP side are only\n");
	fprintf(stderr,"                                   initiated when a TCP connection is accepted\n");
}

#define DFLT_PORT 8192

typedef std::pair<unsigned short, unsigned short> PP;

int
main(int argc, char **argv)
{
const char    *peerIp      = "127.0.0.1";
int            rval        = 1;
int            opt;
Bridge::Flags  flags       = Bridge::NONE;
uint64_t       rssiMsk     = 0;
uint64_t       rssiBit     = 1;

std::vector< PP > ports;

	while ( (opt = getopt(argc, argv, "hCp:u:a:d")) > 0 ) {
		switch (opt) {
			case 'h':
				rval = 0;
			default:
				usage( argv[0] );
				return rval;
			case 'p':
				rssiMsk |= rssiBit;
				// fall through
			case 'u':
				unsigned short me, peer;
				int st;

				if ( 0 == rssiBit ) {
					fprintf(stderr,"Too many -u/-p options; only %d supported\n", (unsigned)sizeof(rssiBit)*8);
					return rval;
				}

				rssiBit <<= 1;

				if ( ::strchr(optarg,':') ) {
					st = (2 != sscanf(optarg, "%hi:%hi", &peer, &me) );
				} else {
					st = (1 != sscanf(optarg, "%hi", &peer));
					me = peer;
				}
				if ( st ) {
					fprintf(stderr,"Error: -%c argument misformed\n", opt);
					return rval;
				}
				for ( std::vector<PP>::const_iterator it=ports.begin(); it != ports.end(); ++it ) {
					if ( it->first == peer || it->second == me ) {
						fprintf(stderr,"Error: ports cannot be used multiple times\n");
						return rval;
					}
				}
				ports.push_back( PP( peer, me ) );	
				break;

			case 'd':
				flags = (Bridge::Flags)(flags | Bridge::DEBUG);
				break;

			case 'C':
				flags = (Bridge::Flags)(flags | Bridge::ALWAYSCONN);
				break;

			case 'a':
				if ( INADDR_NONE == inet_addr( optarg ) ) {
					fprintf(stderr,"Error: invalid IP address in -%c\n", opt);
					return rval;
				}
				peerIp = optarg;
				break;
		}
	}

	if ( ports.empty() ) {
		ports.push_back( PP( DFLT_PORT, DFLT_PORT ) );
	}

try {
	std::vector< shared_ptr<Bridge> > bridges;

	rssiBit = 1;

	for ( std::vector<PP>::const_iterator it = ports.begin(); it != ports.end(); ++it ) {
		Bridge::Flags f = flags;
		if ( (rssiBit & rssiMsk) ) {
			f = (Bridge::Flags)(f | Bridge::RSSI);
		}
		bridges.push_back( make_shared<Bridge>( peerIp, it->first, it->second, f ) );
		rssiBit <<= 1;
	}

	pause();

} catch (CPSWError &e) {
	std::cerr <<  e.getInfo() << "\n";
	throw;
}

	return rval;
}
