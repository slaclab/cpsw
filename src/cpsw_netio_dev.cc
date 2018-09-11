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

//#define NETIO_DEBUG

CNetIODevImpl::CNetIODevImpl(Key &k, const char *name, const char *ip)
: CDevImpl(k, name),
  ip_str_(ip ? ip : "ANY")
{
	if ( INADDR_NONE == ( d_ip_ = ip ? inet_addr( ip ) : INADDR_ANY ) ) {
		throw InvalidArgError( ip );
	}
}

CNetIODevImpl::CNetIODevImpl(Key &k, YamlState &ypath)
: CDevImpl(k, ypath)
{
std::string dummy;

	if ( readNode(ypath, YAML_KEY_ipAddr, &ip_str_) ) {
		if ( INADDR_NONE == ( d_ip_ = inet_addr( ip_str_.c_str() ) ) ) {
			throw InvalidArgError( ip_str_.c_str() );
		}
	} else {
		ip_str_ = std::string("ANY");
		d_ip_   = INADDR_ANY;
	}

	if ( readNode(ypath, YAML_KEY_socksProxy, &dummy) ) {
		fprintf(stderr,"This version of CPSW does not yet support SOCKS proxies (NetIODev/socksProxy); please upgrade.\n");
		throw ConfigurationError("No support for 'socksProxy' in this version of CPSW; please upgrade.");
	}

	if ( readNode(ypath, YAML_KEY_rssiBridge, &dummy) ) {
		fprintf(stderr,"This version of CPSW does not yet 'rssiBridge'; please upgrade.\n");
		throw ConfigurationError("No support for 'rssiBridge' in this version of CPSW; please upgrade.");
	}
}

void
CNetIODevImpl::dumpYamlPart(YAML::Node & node) const
{
	CDevImpl::dumpYamlPart( node );
	writeNode(node, YAML_KEY_ipAddr, ip_str_);
}

void CNetIODevImpl::dump(FILE *f) const
{
	fprintf(f, "Peer: %s", ip_str_.c_str());
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

	getPorts( &myPorts );

ProtoPort              port = bldr->build( myPorts );

shared_ptr<CCommAddressImpl> addr;


	switch( bldr->getSRPVersion() ) {
		case IProtoStackBuilder::SRP_UDP_NONE:
#ifdef NETIO_DEBUG
			fprintf(stderr,"Creating CCommAddress\n");
#endif
			addr = make_shared<CCommAddressImpl>(key, port);
		break;

		case IProtoStackBuilder::SRP_UDP_V1:
		case IProtoStackBuilder::SRP_UDP_V2:
		case IProtoStackBuilder::SRP_UDP_V3:
#ifdef NETIO_DEBUG
			fprintf(stderr,"Creating SRP address (V %d)\n", bldr->getSRPVersion());
#endif
			addr = make_shared<CSRPAddressImpl>(key, bldr, port);
		break;

		default:
			throw InternalError("Unknown Communication Protocol");
	}

	add(addr, child);
	addr->startProtoStack();
}

void CNetIODevImpl::addAtAddress(Field child, YamlState &ypath)
{
ProtoStackBuilder bldr( IProtoStackBuilder::create( ypath ) );

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
