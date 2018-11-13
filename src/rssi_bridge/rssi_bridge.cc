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

#include <sys/file.h>

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rpcMapService.h>
#include <rpc/rpc.h>


#define RSSI_BR_DEBUG

class CThrottler;
typedef shared_ptr<CThrottler> Throttler;

class CThrottler {
private:
	UdpPort         udpPrt_;
	CMtx            mtx_;
	pthread_cond_t  cond_;

public:
	CThrottler(UdpPort udpPrt)
	: udpPrt_(udpPrt)
	{
	int err;
		if ( (err = pthread_cond_init( &cond_, NULL )) ) {
			throw InternalError("Initializing condvar failed", err);
		}
	}

	void unthrottle()
	{
	int err;
		if ( (err = pthread_cond_signal( &cond_ )) ) {
			throw InternalError("Signalling condvar failed", err);
		}
	}

	void throttle()
	{
		if ( udpPrt_->isFull() ) {
			CMtx::lg GUARD( &mtx_ );
			do {
				pthread_cond_wait( &cond_, mtx_.getp() );
			} while ( udpPrt_->isFull() );
		}
	}

	~CThrottler()
	{
		pthread_cond_destroy( &cond_ );
	}
};

class CMover : public CRunnable {
private:
	ProtoPort to_, from_;
	int       dbg_;
	Throttler throttler_;
	int       throttle_;

	CMover(const CMover&);
	CMover &operator=(const CMover&);
protected:
	virtual void *threadBody();
public:
	CMover(const char *name, ProtoPort to, ProtoPort from, int debug, Throttler throttler, int throttle);

	virtual ~CMover();
};

typedef shared_ptr<CMover> Mover;

class Bridge : public ITcpConnHandler {
private:
	Mover          udpToTcp_;
	Mover          tcpToUdp_;
	ProtoPort      udpTop_;
	ProtoPort      tcpTop_;
	unsigned       flags_;
	unsigned short peerPort_;
	unsigned short myPort_;

public:
	typedef enum { NONE = 0, RSSI=1, DEBUG=2, ALWAYSCONN=4, INFO=8 } Flags;

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

	virtual unsigned short
	getPeerPort()
	{
		return peerPort_;
	}

	virtual unsigned short
	getPort()
	{
		return myPort_;
	}

	void shutdown();

	~Bridge();
};

void*
CMover::threadBody()
{
	while ( 1 ) {
		BufChain bc = from_->pop( NULL );
		if ( bc ) {
			if ( throttle_ > 0 ) {
				throttler_->throttle();
			} else if ( throttle_ < 0 ) {
				throttler_->unthrottle();
			}
#ifdef RSSI_BR_DEBUG
			if ( dbg_ ) {
				printf("%s: got %ld bytes\n", getName().c_str(), (long)bc->getSize());
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

CMover::CMover(const char *name, ProtoPort to, ProtoPort from, int debug, Throttler throttler, int throttle)
: CRunnable ( name         ),
  to_       ( to           ),
  from_     ( from         ),
  dbg_      ( debug        ),
  throttler_( throttler    ),
  throttle_ ( throttle     )
{
}

CMover::~CMover()
{
	threadStop();
}

Bridge::Bridge(const char *peerIp, unsigned short peerPort, unsigned short myPort, Flags flags)
: flags_   ( flags    ),
  peerPort_( peerPort )
{
TcpConnHandler connHdlr = (flags & Bridge::ALWAYSCONN) ? TcpConnHandler() : this;
UdpPort        udpPrt   = IUdpPort::create( NULL, 0, 0, 0, useRssi() ? 10 : 100  );
int            depth    = 4;
bool           isServer = true;
TcpPort        tcpPrt   = ITcpPort::create( NULL, myPort, depth, isServer, connHdlr );
Throttler      thrtlr;

		if ( 0 == myPort ) {
			if ( ( flags & Bridge::INFO ) ) {
				printf("Peer/UDP port %hu served by our TCP port %hu\n", peerPort, tcpPrt->getTcpPort());
			}
		}

		myPort_ = tcpPrt->getTcpPort();

		if ( useRssi() ) {
			udpTop_ = CRssiPort::create( false );
			udpTop_->attach( udpPrt );
		} else {
			udpTop_    = udpPrt;
			thrtlr     = cpsw::make_shared<CThrottler>( udpPrt );
		}

		tcpTop_ = tcpPrt;

		udpPrt->connect( peerIp, peerPort );

		for ( ProtoPort p=tcpTop_; p; p=p->getUpstreamPort() )
			p->start();

		if ( ! connHdlr ) {
			up();
		}

		try {

			udpToTcp_ = cpsw::make_shared<CMover>( "UDP->TCP", tcpTop_, udpTop_, debug(), thrtlr, thrtlr ? -1 : 0 );
			udpToTcp_->threadStart();

			tcpToUdp_ = cpsw::make_shared<CMover>( "TCP->UDP", udpTop_, tcpTop_, debug(), thrtlr, thrtlr ?  1 : 0 );
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
	fprintf(stderr,"Usage: %s [-hdCR] -a <dest_ip> [-p <dst_port>[:<tcp_srv_port>]] [-u <dst_port>[:<tcp_srv_port>]]\n", nm);
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
	fprintf(stderr,"       -R                        : Do not start the RPC service\n");
	fprintf(stderr,"       -v                        : Increase verbosity\n");
}

#define DFLT_PORT 8192
#define DFLT_RSSI 0

typedef std::pair<unsigned short, unsigned short> PP;

static std::vector< PortMap > portMaps;

int
main(int argc, char **argv)
{
const char        *peerIp      = "127.0.0.1";
int                rval        = 1;
int                opt;
Bridge::Flags      flags       = Bridge::NONE;
uint64_t           rssiMsk     = 0;
uint64_t           rssiBit     = 1;
int                useRpc      = 1;
in_addr_t          peer;
const char        *opts        = "hRCp:u:a:dv";
int                verbose     = 0;
struct sockaddr_in mcsa;

std::vector< PP > ports;

	while ( (opt = getopt(argc, argv, opts )) > 0 ) {
		switch (opt ) {
			case 'R':
				useRpc = 0;
			break;

			default:
			break;
		}
	}

	optind = 1;

	if ( useRpc ) {
		if ( rpcProbeRelay( &mcsa ) ) {
			fprintf(stderr,"Unable to contact RELAY service\n");
			return 1;
		}
	}

	while ( (opt = getopt(argc, argv, "hRCp:u:a:d")) > 0 ) {
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
					me = useRpc ? 0 : peer;
				}
				if ( st ) {
					fprintf(stderr,"Error: -%c argument misformed\n", opt);
					return rval;
				}
				for ( std::vector<PP>::const_iterator it=ports.begin(); it != ports.end(); ++it ) {
					if ( it->first == peer || (it->second == me && me != 0) ) {
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
				peerIp = optarg;
				break;
			case 'R':
				/* already done above */
				useRpc = 0;
				break;

			case 'v':
				verbose++;
				break;
		}
	}

	peer = inet_addr( peerIp );
	if ( INADDR_NONE == peer ) {
		fprintf(stderr,"Error: invalid IP address in -%c\n", opt);
		return rval;
	}

	// Allow only a single instance for a given destination IP
	{
	char      fnam[256];
	int       fd;

		snprintf(fnam, sizeof(fnam), "/var/lock/rssi_bridge.%x", ntohl( peer ) );

		if ( (fd = open( fnam, O_RDONLY | O_CREAT, 0444) ) < 0 ) {
			perror("Unable to open lock file");
			return rval;
		}

		// Note: POSIX lock (fcntl(F_SETLK, F_WRLCK) or lockf
		//       require write-permission for obtaining an
		//       exclusive lock -- but flock doesnt!
		//       This has the advantage that we don't have
		//       to make the lock file world-writable and yet
		//       any other user may use it (and do denial
		//       of service, of course).

		// Note: the lock is automatically released on exit
		if ( flock( fd, LOCK_EX | LOCK_NB ) ) {
			if ( EWOULDBLOCK == errno ) {
				fprintf(stderr,"ERROR: Another bridge is already running for '%s'\n", peerIp);
			} else {
				perror("Unable to lock");
			}
			return rval;
		}
	}

	if ( ports.empty() ) {
		ports.push_back( PP( DFLT_PORT, useRpc ? 0 : DFLT_PORT ) );
		if ( DFLT_RSSI ) {
			rssiMsk |= 1;
		}
	}

try {
	std::vector< shared_ptr<Bridge> > bridges;

	rssiBit = 1;

	if ( verbose || !useRpc ) {
		flags = (Bridge::Flags)(flags | Bridge::INFO);
	}

	for ( std::vector<PP>::const_iterator it = ports.begin(); it != ports.end(); ++it ) {
		Bridge::Flags f = flags;
		if ( (rssiBit & rssiMsk) ) {
			f = (Bridge::Flags)(f | Bridge::RSSI);
		}
		bridges.push_back( cpsw::make_shared<Bridge>( peerIp, it->first, it->second, f ) );
		shared_ptr<Bridge> b = bridges.back();

		PortMap            m;
		m.reqPort = b->getPeerPort();	
		m.actPort = b->getPort();
		m.flags   = 0;
		if ( b->useRssi() ) {
			m.flags |= MAP_PORT_DESC_FLG_RSSI;
		}
		portMaps.push_back( m );
		
		rssiBit <<= 1;
	}

	if ( useRpc ) {
		SVCXPRT *mapSvc;
		if ( ! (mapSvc = rpcMapServer( &mcsa, peer, &portMaps[0], portMaps.size() )) )
		{
			fprintf(stderr, "Unable to start RPC MAP server\n" );
			return 1;
		}
		svc_run();
	} else {
		pause();
	}

	/* Never get here */

} catch (CPSWError &e) {
	std::cerr <<  e.getInfo() << "\n";
	throw;
}

	return rval;
}
