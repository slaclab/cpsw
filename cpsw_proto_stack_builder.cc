 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#define __STDC_FORMAT_MACROS

#include <inttypes.h>

#include <cpsw_proto_mod_depack.h>
#include <cpsw_proto_mod_udp.h>
#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_proto_mod_rssi.h>
#include <cpsw_proto_mod_tdestmux.h>

#include <cpsw_yaml.h>

#include <boost/make_shared.hpp>

using boost::make_shared;
using boost::dynamic_pointer_cast;

class CProtoStackBuilder : public IProtoStackBuilder {
	private:
		INetIODev::ProtocolVersion protocolVersion_;
		uint64_t                   SRPTimeoutUS_;
		int                        SRPDynTimeout_;
		unsigned                   SRPRetryCount_;
		unsigned                   UdpPort_;
		unsigned                   UdpOutQueueDepth_;
		unsigned                   UdpNumRxThreads_;
		int                        UdpPollSecs_;
		bool                       hasRssi_;
		int                        hasDepack_;
		unsigned                   DepackOutQueueDepth_;
		unsigned                   DepackLdFrameWinSize_;
		unsigned                   DepackLdFragWinSize_;
		int                        hasSRPMux_;
		unsigned                   SRPMuxVirtualChannel_;
		bool                       hasTDestMux_;
		unsigned                   TDestMuxTDEST_;
		int                        TDestMuxStripHeader_;
		unsigned                   TDestMuxOutQueueDepth_;
		in_addr_t                  IPAddr_;
	public:
		virtual void reset()
		{
			protocolVersion_        = SRP_UDP_V2;
			SRPTimeoutUS_           = 0;
			SRPDynTimeout_          = -1;
			SRPRetryCount_          = -1;
			UdpPort_                = 8192;
			UdpOutQueueDepth_       = 0;
			UdpNumRxThreads_        = 0;
			UdpPollSecs_            = -1;
			hasRssi_                = false;
			hasDepack_              = -1;
			DepackOutQueueDepth_    = 0;
			DepackLdFrameWinSize_   = 0;
			DepackLdFragWinSize_    = 0;
			hasSRPMux_              = -1;
			SRPMuxVirtualChannel_   = 0;
			hasTDestMux_            = false;
			TDestMuxTDEST_          = 0;
			TDestMuxStripHeader_    = -1;
			TDestMuxOutQueueDepth_  = 0;
			IPAddr_                 = INADDR_NONE;
		}

		CProtoStackBuilder()
		{
			reset();
		}

		CProtoStackBuilder(YamlState &);

		void setIPAddr(in_addr_t peer)
		{
			IPAddr_ = peer;
		}

		in_addr_t getIPAddr()
		{
			return IPAddr_;
		}


		bool hasSRP()
		{
			return SRP_UDP_NONE != protocolVersion_;
		}

		virtual void            setSRPVersion(ProtocolVersion v)
		{
			if ( SRP_UDP_NONE != v && SRP_UDP_V1 != v && SRP_UDP_V2 != v && SRP_UDP_V3 != v ) {
				throw InvalidArgError("Invalid protocol version");	
			}
			protocolVersion_ = v;
			if ( SRP_UDP_NONE == v ) {
				setSRPTimeoutUS( 0 );
				setSRPRetryCount( -1 );
			}
		}

		virtual ProtocolVersion getSRPVersion()
		{
			return protocolVersion_;
		}

		virtual void            setSRPTimeoutUS(uint64_t v)
		{
			SRPTimeoutUS_ = v;
		}

		virtual uint64_t        getSRPTimeoutUS()
		{
			if ( 0 == SRPTimeoutUS_ )
				return hasRssi() ? 500000 : 10000;
			return SRPTimeoutUS_;
		}

		virtual void            setSRPRetryCount(unsigned v)
		{
			SRPRetryCount_ = v;
		}

		virtual unsigned        getSRPRetryCount()
		{
			if ( (unsigned)-1 == SRPRetryCount_ )
				return 10;
			return SRPRetryCount_;
		}

		virtual bool            hasUdp()
		{
			return getUdpPort() != 0;
		}

		virtual void            useSRPDynTimeout(bool v)
		{
			SRPDynTimeout_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPDynTimeout()
		{
			if ( SRPDynTimeout_ < 0 )
				return ! hasTDestMux();
			return SRPDynTimeout_ ? true : false;
		}

		virtual void            setUdpPort(unsigned v)
		{
			if ( v > 65535 || v == 0 )
				throw InvalidArgError("Invalid UDP Port");
			UdpPort_ = v;
		}

		virtual unsigned        getUdpPort()
		{
			return UdpPort_;
		}

		virtual void            setUdpOutQueueDepth(unsigned v)
		{
			UdpOutQueueDepth_ = v;
		}

		virtual unsigned        getUdpOutQueueDepth()
		{
			if ( 0 == UdpOutQueueDepth_ )
				return 10;
			return UdpOutQueueDepth_;
		}

		virtual void            setUdpNumRxThreads(unsigned v)
		{
			if ( v > 100 )
				throw InvalidArgError("Too many UDP RX threads");
			UdpNumRxThreads_ = v;
		}

		virtual unsigned        getUdpNumRxThreads()
		{
			if ( 0 == UdpNumRxThreads_ )
				return 1;
			return UdpNumRxThreads_;
		}

		virtual void            setUdpPollSecs(int v)
		{
			UdpPollSecs_ = v;
		}

		virtual int             getUdpPollSecs()
		{
			if ( 0 >  UdpPollSecs_ ) {
				if ( ! hasRssi() && ( ! hasSRP() || hasTDestMux() ) )
					return 60;
			}
			return UdpPollSecs_;
		}

		virtual void            useRssi(bool v)
		{
			hasRssi_ = v;
		}

		virtual bool            hasRssi()
		{
			return hasRssi_;
		}

		virtual void            useDepack(bool v)
		{
			if ( ! (hasDepack_ = (v ? 1:0)) ) {
				setDepackOutQueueDepth( 0 );
				setDepackLdFrameWinSize( 0 );
				setDepackLdFragWinSize( 0 );
			}
		}

		virtual bool            hasDepack()
		{
			if ( hasDepack_ < 0 ) {
				return ! hasSRP() || hasTDestMux();

			}
			return hasDepack_ > 0;
		}

		virtual void            setDepackOutQueueDepth(unsigned v)
		{
			DepackOutQueueDepth_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackOutQueueDepth()
		{
			if ( 0 == DepackOutQueueDepth_ )
				return 50;
			return DepackOutQueueDepth_;
		}

		virtual void            setDepackLdFrameWinSize(unsigned v)
		{
			if ( v > 10 )	
				throw InvalidArgError("Requested depacketizer frame window too large");
			DepackLdFrameWinSize_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackLdFrameWinSize()
		{
			if ( 0 == DepackLdFrameWinSize_ )
				return hasRssi() ? 1 : 5;
			return DepackLdFrameWinSize_;
		}

		virtual void            setDepackLdFragWinSize(unsigned v)
		{
			if ( v > 10 )	
				throw InvalidArgError("Requested depacketizer frame window too large");
			DepackLdFragWinSize_ = v;
			useDepack( true );
		}

		virtual unsigned        getDepackLdFragWinSize()
		{
			if ( 0 == DepackLdFragWinSize_ )
				return hasRssi() ? 1 : 5;
			return DepackLdFragWinSize_;
		}

		virtual void            useSRPMux(bool v)
		{
			hasSRPMux_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPMux()
		{
			if ( hasSRPMux_ < 0 )
				return hasSRP();
			return hasSRPMux_ > 0;
		}

		virtual void            setSRPMuxVirtualChannel(unsigned v)
		{
			if ( v > 255 )
				throw InvalidArgError("Requested SRP Mux Virtual Channel out of range");
			useSRPMux( true );
			SRPMuxVirtualChannel_ = v;
		}

		virtual unsigned        getSRPMuxVirtualChannel()
		{
			return SRPMuxVirtualChannel_;
		}

		virtual void            useTDestMux(bool v)
		{
			hasTDestMux_ = v;
		}

		virtual bool            hasTDestMux()
		{
			return hasTDestMux_;
		}

		virtual void            setTDestMuxTDEST(unsigned v)
		{
			if ( v > 255 )
				throw InvalidArgError("Requested TDEST out of range");
			useTDestMux( true );
			TDestMuxTDEST_ = v;
		}

		virtual unsigned        getTDestMuxTDEST()
		{
			return TDestMuxTDEST_;
		}

		virtual void            setTDestMuxStripHeader(bool v)
		{
			TDestMuxStripHeader_    = (v ? 1:0);
			useTDestMux( true );
		}

		virtual bool            getTDestMuxStripHeader()
		{
			if ( 0 > TDestMuxStripHeader_ ) {
				return hasSRP();
			}
			return TDestMuxStripHeader_ > 0;
		}

		virtual void            setTDestMuxOutQueueDepth(unsigned v)
		{
			TDestMuxOutQueueDepth_ = v;			
			useTDestMux( true );
		}

		virtual unsigned        getTDestMuxOutQueueDepth()
		{
			if ( 0 == TDestMuxOutQueueDepth_ )
				return hasSRP() ? 1 : 50;
			return TDestMuxOutQueueDepth_;
		}

		virtual ProtoStackBuilder clone()
		{
			return make_shared<CProtoStackBuilder>( *this );
		}

		virtual ProtoPort build( std::vector<ProtoPort>& );

		static ProtoPort findProtoPort( ProtoPortMatchParams *, std::vector<ProtoPort>& );

		static ProtoStackBuilder  create(YamlState &ypath);
		static ProtoStackBuilder  create();
};

ProtoStackBuilder
CProtoStackBuilder::create(YamlState &node)
{
ProtoStackBuilder          bldr( IProtoStackBuilder::create() );
unsigned                   u;
uint64_t                   u64;
bool                       b;
INetIODev::ProtocolVersion proto_vers;
int                        i;

	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_SRP) );
		if( nn )
		{
			// initialize proto_vers to silence rhel compiler warning
			// about potentially un-initialized 'proto_vers'
			proto_vers = bldr->getSRPVersion();
			if ( readNode(nn, YAML_KEY_protocolVersion, &proto_vers) )
				bldr->setSRPVersion( proto_vers );
			// initialize u64 to silence rhel compiler warning
			// about potentially un-initialized 'u64'
			u64 = bldr->getSRPTimeoutUS();
			if ( readNode(nn, YAML_KEY_timeoutUS, &u64) )
				bldr->setSRPTimeoutUS( u64 );
			if ( readNode(nn, YAML_KEY_dynTimeout, &b) )
				bldr->useSRPDynTimeout( b );
			if ( readNode(nn, YAML_KEY_retryCount, &u) )
				bldr->setSRPRetryCount( u );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_UDP) );
		if ( nn )
		{
			if ( readNode(nn, YAML_KEY_port, &u) )
				bldr->setUdpPort( u );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setUdpOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_numRxThreads, &u) )
				bldr->setUdpNumRxThreads( u );
			// initialize i to silence rhel compiler warning
			// about potentially un-initialized 'i'
			i = bldr->getUdpPollSecs();
			if ( readNode(nn, YAML_KEY_pollSecs, &i) )
				bldr->setUdpPollSecs( i );
		}
	}
	{
        const YAML::PNode &nn( node.lookup(YAML_KEY_RSSI) );

        bldr->useRssi( !!nn );
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_depack) );
		if (nn )
		{
			bldr->useDepack( true );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setDepackOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_ldFrameWinSize, &u) )
				bldr->setDepackLdFrameWinSize( u );
			if ( readNode(nn, YAML_KEY_ldFragWinSize, &u) )
				bldr->setDepackLdFragWinSize( u );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_SRPMux) );
		if (nn )
		{
			bldr->useSRPMux( true );
			if ( readNode(nn, YAML_KEY_virtualChannel, &u) )
				bldr->setSRPMuxVirtualChannel( u );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_TDESTMux) );
		if (nn )
		{
			bldr->useTDestMux( true );
			if ( readNode(nn, YAML_KEY_TDEST, &u) )
				bldr->setTDestMuxTDEST( u );
			if ( readNode(nn, YAML_KEY_stripHeader, &b) )
				bldr->setTDestMuxStripHeader( b );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setTDestMuxOutQueueDepth( u );
		}
	}

	return bldr;
}

ProtoStackBuilder
IProtoStackBuilder::create(YamlState &node)
{
	return CProtoStackBuilder::create( node );
}

ProtoStackBuilder CProtoStackBuilder::create()
{
	return make_shared<CProtoStackBuilder>();
}

ProtoStackBuilder IProtoStackBuilder::create()
{
	return CProtoStackBuilder::create();
}

ProtoPort CProtoStackBuilder::findProtoPort(ProtoPortMatchParams *cmp, std::vector<ProtoPort> &existingPorts )
{
std::vector<ProtoPort>::const_iterator it;
int                                    requestedMatches = cmp->requestedMatches();
	for ( it = existingPorts.begin(); it != existingPorts.end(); ++it )
	{
		// 'match' modifies the parameters (storing results)
		cmp->reset();
		if ( requestedMatches == cmp->findMatches( *it ) )
			return *it;
	}
	return ProtoPort();
}

ProtoPort CProtoStackBuilder::build( std::vector<ProtoPort> &existingPorts )
{
ProtoPort            rval;
ProtoPort            foundTDestPort;
ProtoPortMatchParams cmp;
ProtoModTDestMux     tDestMuxMod;
ProtoModSRPMux       srpMuxMod;
ProtoStackBuilder    bldr   = clone();
bool                 hasSRP = IProtoStackBuilder::SRP_UDP_NONE != bldr->getSRPVersion();

	// sanity checks
	if ( ! bldr->hasUdp() || (INADDR_NONE == bldr->getIPAddr()) )
		throw ConfigurationError("Currently only UDP transport supported\n");

	if ( ! hasSRP && bldr->hasSRPMux() ) {
		throw ConfigurationError("Cannot configure SRP Demuxer w/o SRP protocol version");
	}

	cmp.udpDestPort_ = bldr->getUdpPort();

#ifdef PSBLDR_DEBUG
	printf("makeProtoPort for port %d\n", bldr->getUdpPort());
#endif

	if ( findProtoPort( &cmp, existingPorts ) ) {

		if ( ! bldr->hasTDestMux() && ( !hasSRP || !bldr->hasSRPMux() ) ) {
			throw ConfigurationError("Some kind of demuxer must be used when sharing a UDP port");
		}

#ifdef PSBLDR_DEBUG
	printf("makeProtoPort port %d found\n", bldr->getUdpPort());
#endif

		// existing RSSI configuration must match the requested one
		if ( bldr->hasRssi() ) {
#ifdef PSBLDR_DEBUG
	printf("  including RSSI\n");
#endif
			cmp.haveRssi_.include();
		} else {
#ifdef PSBLDR_DEBUG
	printf("  excluding RSSI\n");
#endif
			cmp.haveRssi_.exclude();
		}

		// existing DEPACK configuration must match the requested one
		if ( bldr->hasDepack() ) {
			cmp.haveDepack_.include();
#ifdef PSBLDR_DEBUG
	printf("  including depack\n");
#endif
		} else {
			cmp.haveDepack_.exclude();
#ifdef PSBLDR_DEBUG
	printf("  excluding depack\n");
#endif
		}

		if ( bldr->hasTDestMux() ) {
			cmp.tDest_ = bldr->getTDestMuxTDEST();
#ifdef PSBLDR_DEBUG
	printf("  tdest %d\n", bldr->getTDestMuxTDEST());
#endif
		} else {
			cmp.tDest_.exclude();
#ifdef PSBLDR_DEBUG
	printf("  tdest excluded\n");
#endif
		}

		if ( (foundTDestPort = findProtoPort( &cmp, existingPorts )) ) {
#ifdef PSBLDR_DEBUG
	printf("  tdest port FOUND\n");
#endif

			// either no tdest demuxer or using an existing tdest port
			if ( ! hasSRP ) {
				throw ConfigurationError("Cannot share TDEST w/o SRP demuxer");
			}

			cmp.srpVersion_     = bldr->getSRPVersion();
			cmp.srpVC_          = bldr->getSRPMuxVirtualChannel();

			if ( findProtoPort( &cmp, existingPorts ) ) {
				throw ConfigurationError("SRP VC already in use");
			}

			cmp.srpVC_.wildcard();

			if ( ! findProtoPort( &cmp, existingPorts ) ) {
				throw ConfigurationError("No SRP Demultiplexer found -- cannot create SRP port on top of existing protocol modules");
			}

			srpMuxMod = dynamic_pointer_cast<ProtoModSRPMux::element_type>( cmp.srpVC_.handledBy_ );

			if ( ! srpMuxMod ) {
				throw InternalError("No SRP Demultiplexer - but there should be one");
			}

		} else {
			// possibilities here are
			//   asked for no tdest demux but there is one present
			//   asked for tdest demux on non-existing port (OK)
			//   other mismatches
			if ( bldr->hasTDestMux() ) {
				cmp.tDest_.wildcard();
				if ( ! findProtoPort( &cmp, existingPorts ) ) {
					throw ConfigurationError("No TDEST Demultiplexer found");
				}
				tDestMuxMod  = dynamic_pointer_cast<ProtoModTDestMux::element_type>( cmp.tDest_.handledBy_ );

				if ( ! tDestMuxMod ) {
					throw InternalError("No TDEST Demultiplexer - but there should be one");
				}
#ifdef PSBLDR_DEBUG
	printf("  using (existing) tdest MUX\n");
#endif
			} else {
				throw ConfigurationError("Unable to create new port on existing protocol modules");
			}
		}
	} else {
		// create new
		struct sockaddr_in dst;

		dst.sin_family      = AF_INET;
		dst.sin_port        = htons( bldr->getUdpPort() );
		dst.sin_addr.s_addr = bldr->getIPAddr();

		// Note: UDP module MUST have a queue if RSSI is used
		rval = CShObj::create< ProtoModUdp >( &dst, bldr->getUdpOutQueueDepth(), bldr->getUdpNumRxThreads(), bldr->getUdpPollSecs() );

		if ( bldr->hasRssi() ) {
#ifdef PSBLDR_DEBUG
	printf("  creating RSSI\n");
#endif
			ProtoModRssi rssi = CShObj::create<ProtoModRssi>();
			rval->addAtPort( rssi );
			rval = rssi;
		}

		if ( bldr->hasDepack() ) {
#ifdef PSBLDR_DEBUG
	printf("  creating depack\n");
#endif
			ProtoModDepack depackMod  = CShObj::create< ProtoModDepack > (
			                                bldr->getDepackOutQueueDepth(),
			                                bldr->getDepackLdFrameWinSize(),
			                                bldr->getDepackLdFragWinSize(),
			                                CTimeout() );
			rval->addAtPort( depackMod );
			rval = depackMod;
		}
	}

	if ( bldr->hasTDestMux()  && ! foundTDestPort ) {
#ifdef PSBLDR_DEBUG
	printf("  creating tdest port\n");
#endif
		if ( ! tDestMuxMod ) {
			tDestMuxMod = CShObj::create< ProtoModTDestMux >();
			rval->addAtPort( tDestMuxMod );
		}
		rval = tDestMuxMod->createPort( bldr->getTDestMuxTDEST(), bldr->getTDestMuxStripHeader(), bldr->getTDestMuxOutQueueDepth() );
	}

	if ( bldr->hasSRPMux() ) {
		if ( ! srpMuxMod ) {
#ifdef PSBLDR_DEBUG
	printf("  creating SRP mux module\n");
#endif
			srpMuxMod   = CShObj::create< ProtoModSRPMux >( bldr->getSRPVersion() );
			rval->addAtPort( srpMuxMod );
		}
		rval = srpMuxMod->createPort( bldr->getSRPMuxVirtualChannel() );
#ifdef PSBLDR_DEBUG
	printf("  creating SRP mux port\n");
#endif
	}

	return rval;
}
