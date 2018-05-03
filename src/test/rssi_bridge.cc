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

#include <arpa/inet.h>
#include <boost/make_shared.hpp>

#include <unistd.h>

using boost::make_shared;

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

class Bridge {
private:
	Mover     udpToTcp_;
	Mover     tcpToUdp_;
	ProtoPort udpTop_;
	ProtoPort tcpTop_;
	unsigned  flags_;

public:
	typedef enum { NONE = 0, RSSI=1, DEBUG=2 } Flags;

	Bridge(const char *peerIp, unsigned short peerPort, unsigned short myPort, Flags flags);

	bool useRssi()
	{
		return !!(flags_ & RSSI);
	}

	bool debug()
	{
		return !!(flags_ & DEBUG);
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
: flags_(flags)
{
UdpPort   udpPrt  = IUdpPort::create( NULL, 0, 0, 0, useRssi() ? 10 : 100  );
TcpPort   tcpPrt  = ITcpPort::create( NULL, myPort );
Throttler thrtlr;

		if ( useRssi() ) {
			udpTop_ = CRssiPort::create( false );
			udpTop_->attach( udpPrt );
		} else {
			udpTop_    = udpPrt;
			thrtlr     = make_shared<CThrottler>( udpPrt );
		}

		tcpTop_ = tcpPrt;

		udpPrt->connect( peerIp, peerPort );

		for ( ProtoPort p=udpTop_; p; p=p->getUpstreamPort() )
			p->start();
		for ( ProtoPort p=tcpTop_; p; p=p->getUpstreamPort() )
			p->start();

		try {

			udpToTcp_ = make_shared<CMover>( "UDP->TCP", tcpTop_, udpTop_, debug(), thrtlr, thrtlr ? -1 : 0 );
			udpToTcp_->threadStart();

			tcpToUdp_ = make_shared<CMover>( "TCP->UDP", udpTop_, tcpTop_, debug(), thrtlr, thrtlr ?  1 : 0 );
			tcpToUdp_->threadStart();

		} catch (...) {
			shutdown();
			throw;
		}
};

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
	fprintf(stderr,"Usage: %s [-hrd] -a <dest_ip> [-p <dst_port>[:<tcp_srv_port>]]\n", nm);
	fprintf(stderr,"       RSSI <-> TCP bridge\n");
	fprintf(stderr,"       -a <dest_ip>              : remote address where a RSSI server is listening.\n");
	fprintf(stderr,"       -p <dst_port>[:<tcp_port>]: <dst_port> identifies the remote UDP port and <tcp_port>\n");
	fprintf(stderr,"                                   the local TCP port where this program is listening. It\n");
	fprintf(stderr,"                                   defaults to <dst_port>.\n");
	fprintf(stderr,"                                   Note that multiple -p options may be given in order to\n");
	fprintf(stderr,"                                   bridge multiple connections.\n");
	fprintf(stderr,"       -h                        : This message.\n");
	fprintf(stderr,"       -r                        : Disable RSSI. Use pure UDP.\n");
#ifdef RSSI_BR_DEBUG
	fprintf(stderr,"       -d                        : Enable debug messages\n");
#else
	fprintf(stderr,"       -d                        : Debug support not compiled\n");
#endif
}

#define DFLT_PORT 8192

typedef std::pair<unsigned short, unsigned short> PP;

int
main(int argc, char **argv)
{
const char    *peerIp      = "127.0.0.1";
int            rval        = 1;
int            opt;
Bridge::Flags  flags       = Bridge::RSSI;

std::vector< PP > ports;

	while ( (opt = getopt(argc, argv, "hp:a:dr")) > 0 ) {
		switch (opt) {
			case 'h':
				rval = 0;
			default:
				usage( argv[0] );
				return rval;
			case 'p':
				unsigned short me, peer;
				int st;
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

			case 'r':
				flags = (Bridge::Flags)(flags & ~Bridge::RSSI);
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

	for ( std::vector<PP>::const_iterator it = ports.begin(); it != ports.end(); ++it ) {
		bridges.push_back( make_shared<Bridge>( peerIp, it->first, it->second, flags ) );
	}

	pause();

} catch (CPSWError &e) {
	std::cerr <<  e.getInfo() << "\n";
	throw;
}

	return rval;
}
