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
#include <cpsw_proto_mod_tcp.h>
#include <cpsw_proto_mod_srpmux.h>
#include <cpsw_proto_mod_rssi.h>
#include <cpsw_proto_mod_tdestmux.h>

#include <cpsw_yaml.h>

#include <boost/make_shared.hpp>

using boost::make_shared;
using boost::dynamic_pointer_cast;

//#define PSBLDR_DEBUG

class CProtoStackBuilder : public IProtoStackBuilder {
	public:
		typedef enum TransportProto { NONE = 0, UDP = 1, TCP = 2 } TransportProto;
	private:
		INetIODev::ProtocolVersion protocolVersion_;
		uint64_t                   SRPTimeoutUS_;
		int                        SRPDynTimeout_;
		unsigned                   SRPRetryCount_;
		TransportProto             Xprt_;
		unsigned                   XprtPort_;
		unsigned                   XprtOutQueueDepth_;
        int                        UdpThreadPriority_;
		unsigned                   UdpNumRxThreads_;
		int                        UdpPollSecs_;
        int                        TcpThreadPriority_;
		bool                       hasRssi_;
        int                        RssiThreadPriority_;
		int                        hasDepack_;
		unsigned                   DepackOutQueueDepth_;
		unsigned                   DepackLdFrameWinSize_;
		unsigned                   DepackLdFragWinSize_;
		int                        DepackThreadPriority_;
		int                        hasSRPMux_;
		unsigned                   SRPMuxVirtualChannel_;
		unsigned                   SRPMuxOutQueueDepth_;
		int                        SRPMuxThreadPriority_;
		bool                       hasTDestMux_;
		unsigned                   TDestMuxTDEST_;
		int                        TDestMuxStripHeader_;
		unsigned                   TDestMuxOutQueueDepth_;
		int                        TDestMuxThreadPriority_;
		in_addr_t                  IPAddr_;
	public:
		virtual void reset()
		{
			protocolVersion_        = SRP_UDP_V2;
			SRPTimeoutUS_           = 0;
			SRPDynTimeout_          = -1;
			SRPRetryCount_          = -1;
			Xprt_                   = UDP;
			XprtPort_               = 8192;
			XprtOutQueueDepth_      = 0;
			UdpThreadPriority_      = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			UdpNumRxThreads_        = 0;
			UdpPollSecs_            = -1;
			TcpThreadPriority_      = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasRssi_                = false;
			hasDepack_              = -1;
			RssiThreadPriority_     = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			DepackOutQueueDepth_    = 0;
			DepackLdFrameWinSize_   = 0;
			DepackLdFragWinSize_    = 0;
			DepackThreadPriority_   = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasSRPMux_              = -1;
			SRPMuxOutQueueDepth_    = 0;
			SRPMuxVirtualChannel_   = 0;
			SRPMuxThreadPriority_   = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
			hasTDestMux_            = false;
			TDestMuxTDEST_          = 0;
			TDestMuxStripHeader_    = -1;
			TDestMuxOutQueueDepth_  = 0;
			TDestMuxThreadPriority_ = IProtoStackBuilder::DFLT_THREAD_PRIORITY;
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
				return hasRssi() ? 500000 : (getTcpPort() > 0 ? 4000000 : 10000);
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

		virtual bool            hasTcp()
		{
			return getTcpPort() != 0;
		}

		virtual void            useSRPDynTimeout(bool v)
		{
			SRPDynTimeout_ = (v ? 1 : 0);
		}

		virtual bool            hasSRPDynTimeout()
		{
			if ( hasRssi() || getTcpPort() > 0 )
				return false;
			if ( SRPDynTimeout_ < 0 )
				return ! hasTDestMux();
			return SRPDynTimeout_ ? true : false;
		}

		virtual void setXprtPort(unsigned v)
		{
			if ( v > 65535 || v == 0 )
				throw InvalidArgError("Invalid UDP Port");
			XprtPort_ = v;
		}

		virtual void            setUdpPort(unsigned v)
		{
			setXprtPort(v);
			Xprt_ = UDP;
		}

		virtual void            setTcpPort(unsigned v)
		{
			setXprtPort(v);
			Xprt_ = TCP;
		}

		virtual void            setTcpThreadPriority(int prio)
		{
			TcpThreadPriority_ = prio;
		}

		virtual int             getTcpThreadPriority()
		{
			return TcpThreadPriority_;
		}

		virtual void            setUdpThreadPriority(int prio)
		{
			UdpThreadPriority_ = prio;
		}

		virtual int             getUdpThreadPriority()
		{
			return UdpThreadPriority_;
		}


		virtual unsigned        getUdpPort()
		{
			return Xprt_ == UDP ? XprtPort_ : 0;
		}

		virtual unsigned        getTcpPort()
		{
			return Xprt_ == TCP ? XprtPort_ : 0;
		}

		virtual void            setUdpOutQueueDepth(unsigned v)
		{
			XprtOutQueueDepth_ = v;
			Xprt_              = UDP;
		}

		virtual void            setTcpOutQueueDepth(unsigned v)
		{
			XprtOutQueueDepth_ = v;
			Xprt_              = TCP;
		}


		virtual unsigned        getUdpOutQueueDepth()
		{
			if ( 0 == XprtOutQueueDepth_ )
				return 10;
			return XprtOutQueueDepth_;
		}

		virtual unsigned        getTcpOutQueueDepth()
		{
			return getUdpOutQueueDepth();
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
			return hasRssi_ && hasUdp();
		}

		virtual void            setRssiThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useRssi( true );
			}
			RssiThreadPriority_ = prio;
		}

		virtual int             getRssiThreadPriority()
		{
			return hasRssi() ? RssiThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
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

		virtual void            setDepackThreadPriority(int prio)
		{
			DepackThreadPriority_ = prio;
			useDepack( true );
		}

		virtual int             getDepackThreadPriority()
		{
			return hasDepack() ? DepackThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
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
			if ( v > 127 )
				throw InvalidArgError("Requested SRP Mux Virtual Channel out of range");
			useSRPMux( true );
			SRPMuxVirtualChannel_ = v;
		}

		virtual unsigned        getSRPMuxVirtualChannel()
		{
			return SRPMuxVirtualChannel_;
		}

		virtual void            setSRPMuxOutQueueDepth(unsigned v)
		{
			SRPMuxOutQueueDepth_ = v;
			useSRPMux( true );
		}

		virtual unsigned        getSRPMuxOutQueueDepth()
		{
			if ( 0 == SRPMuxOutQueueDepth_ )
				return 50;
			return SRPMuxOutQueueDepth_;
		}

		virtual void            setSRPMuxThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useSRPMux( true  );
			}
			SRPMuxThreadPriority_ = prio;
		}

		virtual int             getSRPMuxThreadPriority()
		{
			return hasSRPMux() ? SRPMuxThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
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
				return 50;
			return TDestMuxOutQueueDepth_;
		}

		virtual void            setTDestMuxThreadPriority(int prio)
		{
			if ( prio != IProtoStackBuilder::NO_THREAD_PRIORITY ) {
				useTDestMux( true  );
			}
			TDestMuxThreadPriority_ = prio;
		}

		virtual int             getTDestMuxThreadPriority()
		{
			return hasTDestMux() ? TDestMuxThreadPriority_ : IProtoStackBuilder::NO_THREAD_PRIORITY;
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
		if ( !!nn && nn.IsMap() )
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
		const YAML::PNode &nn( node.lookup(YAML_KEY_TCP) );
		if ( !!nn && nn.IsMap() )
		{
			if ( ! readNode(nn, YAML_KEY_instantiate, &b) || b ) {
				if ( readNode(nn, YAML_KEY_port, &u) )
					bldr->setTcpPort( u );
				if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
					bldr->setTcpOutQueueDepth( u );
				if ( readNode(nn, YAML_KEY_threadPriority, &i) )
					bldr->setTcpThreadPriority( i );
			}
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_UDP) );
		if ( !!nn && nn.IsMap() )
		{
			if ( ! readNode(nn, YAML_KEY_instantiate, &b) || b ) {
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
				if ( readNode(nn, YAML_KEY_threadPriority, &i) )
					bldr->setUdpThreadPriority( i );
			}
		}
	}
	{
        const YAML::PNode &nn( node.lookup(YAML_KEY_RSSI) );

        bldr->useRssi( !!nn );

		if ( !!nn && nn.IsMap() ) {
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				bldr->setRssiThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				bldr->useRssi( b );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_depack) );
		if ( !!nn && nn.IsMap() )
		{
			bldr->useDepack( true );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setDepackOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_ldFrameWinSize, &u) )
				bldr->setDepackLdFrameWinSize( u );
			if ( readNode(nn, YAML_KEY_ldFragWinSize, &u) )
				bldr->setDepackLdFragWinSize( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				bldr->setDepackThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				bldr->useDepack( b );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_SRPMux) );
		if ( !!nn && nn.IsMap() )
		{
			bldr->useSRPMux( true );
			if ( readNode(nn, YAML_KEY_virtualChannel, &u) )
				bldr->setSRPMuxVirtualChannel( u );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setSRPMuxOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				bldr->setSRPMuxThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				bldr->useSRPMux( b );
		}
	}
	{
		const YAML::PNode &nn( node.lookup(YAML_KEY_TDESTMux) );
		if ( !!nn && nn.IsMap() )
		{
			bldr->useTDestMux( true );
			if ( readNode(nn, YAML_KEY_TDEST, &u) )
				bldr->setTDestMuxTDEST( u );
			if ( readNode(nn, YAML_KEY_stripHeader, &b) )
				bldr->setTDestMuxStripHeader( b );
			if ( readNode(nn, YAML_KEY_outQueueDepth, &u) )
				bldr->setTDestMuxOutQueueDepth( u );
			if ( readNode(nn, YAML_KEY_threadPriority, &i) )
				bldr->setTDestMuxThreadPriority( i );
			if ( readNode(nn, YAML_KEY_instantiate, &b) )
				bldr->useTDestMux( b );
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

#ifdef PSBLDR_DEBUG
	printf("makeProtoPort...entering\n");
#endif

	// sanity checks
	if ( ( ! bldr->hasUdp() && ! bldr->hasTcp() ) || (INADDR_NONE == bldr->getIPAddr()) )
		throw ConfigurationError("Currently only UDP or TCP transport supported\n");

	if ( ! hasSRP && bldr->hasSRPMux() ) {
		throw ConfigurationError("Cannot configure SRP Demuxer w/o SRP protocol version");
	}

	if ( bldr->hasUdp() )
		cmp.udpDestPort_ = bldr->getUdpPort();
	else
		cmp.tcpDestPort_ = bldr->getTcpPort();

#ifdef PSBLDR_DEBUG
	printf("makeProtoPort for %s port %d\n", bldr->hasUdp() ? "UDP" : "TCP" , bldr->hasUdp() ? bldr->getUdpPort() : bldr->getTcpPort());
#endif

	if ( findProtoPort( &cmp, existingPorts ) ) {

		if ( ! bldr->hasTDestMux() && ( !hasSRP || !bldr->hasSRPMux() ) ) {
			throw ConfigurationError("Some kind of demuxer must be used when sharing a UDP port");
		}

#ifdef PSBLDR_DEBUG
	printf("makeProtoPort %s port %d found\n", bldr->hasUdp() ? "UDP" : "TCP" , bldr->hasUdp() ? bldr->getUdpPort() : bldr->getTcpPort());
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
		dst.sin_port        = htons( bldr->hasUdp() ? bldr->getUdpPort() : bldr->getTcpPort() );
		dst.sin_addr.s_addr = bldr->getIPAddr();

		if ( bldr->hasUdp() ) {
			// Note: transport module MUST have a queue if RSSI is used
			rval = CShObj::create< ProtoModUdp >( &dst,
			                                       bldr->getUdpOutQueueDepth(),
			                                       bldr->getUdpThreadPriority(),
			                                       bldr->getUdpNumRxThreads(),
			                                       bldr->getUdpPollSecs()
			);
		} else {
			rval = CShObj::create< ProtoModTcp >( &dst,
			                                       bldr->getTcpOutQueueDepth(),
			                                       bldr->getTcpThreadPriority()
			);
		}

		if ( bldr->hasRssi() ) {
#ifdef PSBLDR_DEBUG
	printf("  creating RSSI\n");
#endif
			ProtoModRssi rssi = CShObj::create<ProtoModRssi>(
			                                bldr->getRssiThreadPriority()
			);
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
			                                CTimeout(),
			                                bldr->getDepackThreadPriority());
			rval->addAtPort( depackMod );
			rval = depackMod;
		}
	}

	if ( bldr->hasTDestMux()  && ! foundTDestPort ) {
#ifdef PSBLDR_DEBUG
	printf("  creating tdest port\n");
#endif
		if ( ! tDestMuxMod ) {
			tDestMuxMod = CShObj::create< ProtoModTDestMux >( bldr->getTDestMuxThreadPriority() );
			rval->addAtPort( tDestMuxMod );
		}
		rval = tDestMuxMod->createPort( bldr->getTDestMuxTDEST(), bldr->getTDestMuxStripHeader(), bldr->getTDestMuxOutQueueDepth() );
	}

	if ( bldr->hasSRPMux() ) {
		if ( ! srpMuxMod ) {
#ifdef PSBLDR_DEBUG
			printf("  creating SRP mux module\n");
#endif
			srpMuxMod   = CShObj::create< ProtoModSRPMux >( bldr->getSRPVersion(), bldr->getSRPMuxThreadPriority() );
			rval->addAtPort( srpMuxMod );
		}
		// reserve enough queue depth - must potentially hold replies to synchronous retries
		// until the synchronous reader comes along for the next time!
		unsigned retryCount = bldr->getSRPRetryCount() & 0xffff; // undocumented hack to test byte-resolution access
		rval = srpMuxMod->createPort( bldr->getSRPMuxVirtualChannel(), 2 * retryCount );
#ifdef PSBLDR_DEBUG
	printf("  creating SRP mux port\n");
#endif
	}

	return rval;
}
