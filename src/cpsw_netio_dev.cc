 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS

#include <cpsw_netio_dev.h>
#include <inttypes.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cpsw_comm_addr.h>
#include <cpsw_srp_addr.h>

#include <cpsw_yaml.h>
#include <cpsw_stdio.h>

#include <netdb.h>

//#define NETIO_DEBUG

CNetIODevImpl::CNetIODevImpl(Key &k, const char *name, const char *ip)
: CDevImpl       (k, name),
  ip_str_        (ip ? ip : "ANY"),
  rssi_bridge_ip_( INADDR_NONE )
{
	socks_proxy_.version = SOCKS_VERSION_NONE;
	if ( INADDR_NONE == ( d_ip_ = ip ? inet_addr( ip ) : INADDR_ANY ) ) {
		throw InvalidArgError( ip );
	}
}

CNetIODevImpl::CNetIODevImpl(Key &k, YamlState &ypath)
: CDevImpl       (k, ypath     ),
  rssi_bridge_ip_( INADDR_NONE )
{
union {
	struct sockaddr    sa;
	struct sockaddr_in sin;
}               addr;
int             err;

	socks_proxy_.version = SOCKS_VERSION_NONE;

	if ( readNode(ypath, YAML_KEY_ipAddr, &ip_str_) ) {
		if ( (err = libSocksGetByName( ip_str_.c_str(), 0, &addr.sa )) ) {
			std::string msg = std::string("ERROR: Invalid ipAddr value; getaddrinfo returned") + std::string( gai_strerror( err ) );
			fprintf(CPSW::fErr(), "%s\n", msg.c_str());
			throw InvalidArgError( msg );
		}
		d_ip_ = addr.sin.sin_addr.s_addr;
	} else {
		ip_str_ = std::string("ANY");
		d_ip_   = INADDR_ANY;
	}
	if ( readNode(ypath, YAML_KEY_socksProxy, &socks_proxy_str_) ) {
		if ( libSocksStrToProxy( &socks_proxy_, socks_proxy_str_.c_str() ) ) {
			throw InvalidArgError( socks_proxy_str_.c_str() );
		}
	} else {
		const char *fromEnv = getenv("SOCKS_PROXY");
		if ( fromEnv && libSocksStrToProxy( &socks_proxy_, fromEnv ) ) {
			throw InvalidArgError( "Invalid proxy from env-var SOCKS_PROXY" );
		}
	}
	if ( readNode(ypath, YAML_KEY_rssiBridge, &rssi_bridge_str_) ) {
		if ( (err = libSocksGetByName( rssi_bridge_str_.c_str(), 0, &addr.sa )) ) {
			std::string msg = std::string("Invalid rssiBridge value; getaddrinfo returned") + std::string( gai_strerror( err ) );
			throw InvalidArgError( msg );
		}
		rssi_bridge_ip_ = addr.sin.sin_addr.s_addr;
	}
}

void
CNetIODevImpl::dumpYamlPart(YAML::Node & node) const
{
	CDevImpl::dumpYamlPart( node );
	writeNode(node, YAML_KEY_ipAddr, ip_str_);
	if ( socks_proxy_str_.length() > 0 ) {
		writeNode(node, YAML_KEY_socksProxy, socks_proxy_str_);
	}
	if ( rssi_bridge_str_.length() > 0 ) {
		writeNode(node, YAML_KEY_rssiBridge, rssi_bridge_str_);
	}
}

void CNetIODevImpl::dump(FILE *f) const
{
	fprintf(f, "Peer: %s", ip_str_.c_str());
}

static int operator!=(const struct sockaddr_in &a, const struct sockaddr_in &b)
{
	return a.sin_family != b.sin_family || a.sin_addr.s_addr != b.sin_addr.s_addr || a.sin_port != b.sin_port;
}

static int operator!=(const LibSocksProxy &a, const LibSocksProxy &b)
{
	return a.version != b.version || a.address.sin != b.address.sin;
}


void CNetIODevImpl::addAtAddress(Field child, ProtoStackBuilder bldr)
{
IAddress::AKey         key  = getAKey();
std::vector<ProtoPort> myPorts;

	if ( INADDR_NONE == bldr->getIPAddr() ) {
		bldr->setIPAddr( getIpAddress() );
	} else if ( getIpAddress() != bldr->getIPAddr() ) {
		throw ConfigurationError("CNetIODev: unexpected IP address");
	}

	if ( INADDR_NONE == bldr->getRssiBridgeIPAddr() ) {
		bldr->setRssiBridgeIPAddr( getRssiBridgeIp() );
	} else if ( getRssiBridgeIp() != bldr->getIPAddr() ) {
		throw ConfigurationError("CNetIODev: unexpected RssiBridge IP address");
	}

	if ( bldr->getSocksProxy()->version == SOCKS_VERSION_NONE ) {
		bldr->setSocksProxy( & getSocksProxy() );
	} else if ( getSocksProxy() != * bldr->getSocksProxy() ) {
		throw ConfigurationError("CNetIODev: unexpected SOCKS proxy");
	}

	getPorts( &myPorts );

ProtoPort              port = bldr->build( myPorts );

shared_ptr<CCommAddressImpl> addr;


	switch( bldr->getSRPVersion() ) {
		case IProtoStackBuilder::SRP_UDP_NONE:
#ifdef NETIO_DEBUG
			fprintf(CPSW::fDbg(), "Creating CCommAddress\n");
#endif
			addr = cpsw::make_shared<CCommAddressImpl>(key, port);
		break;

		case IProtoStackBuilder::SRP_UDP_V1:
		case IProtoStackBuilder::SRP_UDP_V2:
		case IProtoStackBuilder::SRP_UDP_V3:
#ifdef NETIO_DEBUG
			fprintf(CPSW::fDbg(), "Creating SRP address (V %d)\n", bldr->getSRPVersion());
#endif
			addr = cpsw::make_shared<CSRPAddressImpl>(key, bldr, port);
		break;

		default:
			throw InternalError("Unknown Communication Protocol");
	}

	add(addr, child);

	if ( bldr->getAutoStart() ) {
		addr->startUp();
	}
}

void CNetIODevImpl::startUp()
{
	if ( ! isStarted() ) {
		CDevImpl::startUp();
	}
}

void CNetIODevImpl::addAtAddress(Field child, YamlState &ypath)
{
ProtoStackBuilder bldr( IProtoStackBuilder::create( ypath ) );

	bldr->setAutoStart( false );

	if ( bldr->hasUdp() || bldr->hasTcp() ) {
		addAtAddress(child, bldr);
	} else {
		CDevImpl::addAtAddress(child, ypath);
	}
}

void CNetIODevImpl::addAtAddress(Field child, INetIODev::ProtocolVersion version, unsigned dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc, bool useRssi, int tDest)
{
ProtoStackBuilder bldr( IProtoStackBuilder::create() );

	bldr->setSRPVersion( version );
	bldr->setSRPTimeoutUS( timeoutUs );
	bldr->setSRPRetryCount( retryCnt );
	bldr->useRssi( useRssi );
	bldr->setSRPMuxVirtualChannel( vc );
	bldr->setUdpPort( dport );
	if ( tDest >= 0 ) {
		bldr->setTDestMuxTDEST( tDest );
	}

	addAtAddress(child, bldr);
}


void CNetIODevImpl::getPorts( std::vector<ProtoPort> *portVec )
{
	portVec->clear();

	Children myChildren = getChildren();

	// construct a vector of all our ports for passing to the stack builder
	for ( Children::element_type::iterator it = myChildren->begin(); it != myChildren->end(); ++it ) {
		shared_ptr<const CCommAddressImpl> child = static_pointer_cast<const CCommAddressImpl>( *it );
		portVec->push_back( child->getProtoStack() );
	}
}

void CNetIODevImpl::addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi, int tDest)
{
ProtoStackBuilder bldr( IProtoStackBuilder::create() );

	bldr->setSRPVersion( IProtoStackBuilder::SRP_UDP_NONE );
	bldr->setUdpPort( dport );
	// ignore depacketizer (input) timeout -- no builder support because not necessary
	bldr->setUdpOutQueueDepth( inQDepth );
	bldr->setUdpNumRxThreads( nUdpThreads );
	bldr->useRssi( useRssi );
	bldr->setDepackOutQueueDepth( inQDepth );
	bldr->setDepackLdFrameWinSize( ldFrameWinSize );
	bldr->setDepackLdFragWinSize( ldFragWinSize );
	if ( tDest >= 0 ) {
		bldr->setTDestMuxTDEST( tDest );
	}

	addAtAddress(child, bldr);
}


NetIODev INetIODev::create(const char *name, const char *ipaddr)
{
	return CShObj::create<NetIODevImpl>(name, ipaddr);
}


void CNetIODevImpl::setLocked()
{
	if ( getLocked() ) {
		throw InternalError("Cannot attach this type of device multiple times -- need to create a new instance");
	}
	CDevImpl::setLocked();
}
