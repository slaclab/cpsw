#ifndef CPSW_YAML_H
#define CPSW_YAML_H

#include <yaml-cpp/yaml.h>
#include <cpsw_api_builder.h>

/* Template specializations to convert YAML nodes to CPSW entries 
 * (ByteOrder, IIntField::Mode, Field, MMIODev, etc.)
 */

namespace YAML {
template<>
struct convert<ByteOrder> {
  static bool decode(const Node& node, ByteOrder& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if( str.compare( "LE" ) == 0 )
      rhs = LE;
    else if (str.compare( "BE" ) == 0 ) 
      rhs = BE;
    else
      rhs = UNKNOWN;

    return true;
  }
};

template<>
struct convert<IField::Cacheable> {
  static bool decode(const Node& node, IField::Cacheable& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if( str.compare( "NOT_CACHEABLE" ) == 0 )
      rhs = IField::NOT_CACHEABLE;
    else if (str.compare( "WT_CACHEABLE" ) == 0 ) 
      rhs = IField::WT_CACHEABLE;
    else if (str.compare( "WB_CACHEABLE" ) == 0 ) 
      rhs = IField::WB_CACHEABLE;
    else
      rhs = IField::UNKNOWN_CACHEABLE;

    return true;
  }
};

template<>
struct convert<IIntField::Mode> {
  static bool decode(const Node& node, IIntField::Mode& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if( str.compare( "RO" ) == 0 )
      rhs = IIntField::RO;
    else if (str.compare( "WO" ) == 0 ) 
#if 0
// without caching and bit-level access at the SRP protocol level we cannot
// support write-only yet.
      rhs = IIntField::WO;
#else
      rhs = IIntField::RW;
#endif
    else if (str.compare( "RW" ) == 0 ) 
      rhs = IIntField::RW;
    else
      return false;

    return true;
  }
};

template<>
struct convert<IntField> {
  static bool decode(const Node& node, IntField& rhs) {
//    if(!node.IsMap() || node.size() >= 3) {
//      return false;
//    }
    IIntField::Builder bldr = IIntField::IBuilder::create();

    std::string name = node["name"].as<std::string>();
    bldr->name( node["name"].as<std::string>().c_str() );
    if( node["sizeBits"] ) {
        bldr->sizeBits( node["sizeBits"].as<uint64_t>() );
    }
    if( node["isSigned"] ) {
        bldr->isSigned( node["isSigned"].as<bool>() );
    }
    if( node["lsBit"] ) {
        bldr->lsBit( node["lsBit"].as<int>() );
    }
    if( node["mode"] ) {
        bldr->mode( node["mode"].as<IIntField::Mode>() );
    }
    if( node["wordSwap"] ) {
        bldr->wordSwap( node["wordSwap"].as<unsigned>() );
    }
    if( node["enums"] ) {
      const YAML::Node& enums = node["enums"];
      MutableEnum e = IMutableEnum::create();
      for( unsigned i = 0; i < enums.size(); i++ )
      {
	e->add( enums[i]["name"].as<std::string>().c_str(), enums[i]["value"].as<uint64_t>() );
      }
      bldr->setEnum( e );
    }

    rhs = bldr->build();

    return true;
  }
};

template<>
struct convert<SequenceCommand> {
  static bool decode(const Node& node, SequenceCommand& rhs) {
//    if(!node.IsMap() || node.size() >= 3) {
//      return false;
//    }

    std::string name;
    ISequenceCommand::commandSequence commandSeq;
    std::vector<std::string> fields;
    std::vector<uint64_t> values;


    //name = "C_";
    //name = name + node["name"].as<std::string>();
    name = node["name"].as<std::string>();

    if( node["sequence"] ) {
        const YAML::Node& seq = node["sequence"];
        for( unsigned i = 0; i < seq.size(); i++ )
        {
            commandSeq.push_back( ISequenceCommand::Cmd( seq[i]["entry"].as<std::string>(), seq[i]["value"].as<uint64_t>() ) );
        }
    }
    else {
        return false;
    }


    rhs = ISequenceCommand::create( name.c_str(), commandSeq );
    return true;
  }
};

template<>
struct convert<MMIODev> {
  static bool decode(const Node& node, MMIODev& rhs) {

    std::string name = node["name"].as<std::string>();
    uint64_t size = node["size"].as<uint64_t>();
    rhs = IMMIODev::create( name.c_str(), size );
    Field f;

    if ( node["MMIODev"] )
    {
      const YAML::Node& mmio = node["MMIODev"];
      for( unsigned i = 0; i < mmio.size(); i++ )
      {
        uint64_t offset = mmio[i]["offset"].as<uint64_t>();
        unsigned nelms   = mmio[i]["nelms"] ? mmio[i]["nelms"].as<unsigned>() : 1;
        uint64_t stride = mmio[i]["stride"] ? mmio[i]["stride"].as<uint64_t>() : IMMIODev::STRIDE_AUTO;
        ByteOrder byteOrder = mmio[i]["ByteOrder"] ? mmio[i]["ByteOrder"].as<ByteOrder>() : UNKNOWN;
        IField::Cacheable cacheable = mmio[i]["cacheable"] ? \
                       mmio[i]["cacheable"].as<IField::Cacheable>() : \
                       IField::UNKNOWN_CACHEABLE;
        f = mmio[i].as<MMIODev>();
        f->setCacheable( cacheable );
        rhs->addAtAddress( f, offset, nelms, stride, byteOrder );
      }
    }

    // This device contains iField
    if ( node["IntField"] )
    {
      const YAML::Node& iField = node["IntField"];
      for( unsigned i = 0; i < iField.size(); i++ )
      {
        uint64_t offset = iField[i]["offset"].as<uint64_t>();
        unsigned nelms  = iField[i]["nelms"] ? iField[i]["nelms"].as<unsigned>() : 1;
        uint64_t stride = iField[i]["stride"] ? iField[i]["stride"].as<uint64_t>() : IMMIODev::STRIDE_AUTO;
        ByteOrder byteOrder = iField[i]["ByteOrder"] ? iField[i]["ByteOrder"].as<ByteOrder>() : UNKNOWN;
        std::string desc = iField[i]["description"].as<std::string>();
        IField::Cacheable cacheable = iField[i]["cacheable"] ? \
                       iField[i]["cacheable"].as<IField::Cacheable>() : \
                       IField::UNKNOWN_CACHEABLE;
        f = iField[i].as<IntField>();
        f->setDescription( desc );
        f->setCacheable( cacheable );
        rhs->addAtAddress( f, offset, nelms, stride, byteOrder );
      }
    }

    if ( node["Commands"] )
    {
      const YAML::Node& commands = node["Commands"];
      for( unsigned i = 0; i < commands.size(); i++ )
      {
	f = commands[i].as<SequenceCommand>();
	rhs->addAtAddress( f, 0 );
      }
    }
    // This device contains other mmio
    return true;
  }
};

template<>
struct convert<INetIODev::ProtocolVersion> {
  static bool decode(const Node& node, INetIODev::ProtocolVersion& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if ( str.compare( "SRP_UDP_V1" ) == 0 )
      rhs = INetIODev::SRP_UDP_V1;
    else if (str.compare( "SRP_UDP_V2" ) == 0 ) 
      rhs = INetIODev::SRP_UDP_V2;
    else if (str.compare( "SRP_UDP_V3" ) == 0 ) 
      rhs = INetIODev::SRP_UDP_V3;
    else if (str.compare( "SRP_UDP_NONE" ) == 0 ) 
      rhs = INetIODev::SRP_UDP_NONE;
    else
      return false;

    return true;
  }
};

template<>
struct convert<INetIODev::PortBuilder> {
  static bool decode(const Node& node, INetIODev::PortBuilder& rhs) {
    rhs = INetIODev::createPortBuilder(); 
    if( node["SRP"] )
    {
      const YAML::Node& SRP = node["SRP"];
      if( SRP["ProtocolVersion"] )
          rhs->setSRPVersion( SRP["ProtocolVersion"].as<INetIODev::ProtocolVersion>() ); 
      if( SRP["TimeoutUS"] )
          rhs->setSRPTimeoutUS( SRP["TimeoutUS"].as<uint64_t>() ); 
      if( SRP["DynTimeout"] )
          rhs->useSRPDynTimeout( SRP["DynTimeout"].as<bool>() ); 
      if( SRP["RetryCount"] )
          rhs->setSRPRetryCount( SRP["RetryCount"].as<unsigned>() ); 
    }
    if ( node["udp"] )
    {
      const YAML::Node& udp = node["udp"];
      if ( udp["port"] )
          rhs->setUdpPort( udp["port"].as<unsigned>() );
      if ( udp["outQueueDepth"] )
          rhs->setUdpOutQueueDepth( udp["outQueueDepth"].as<unsigned>() );
      if ( udp["numRxThreads"]  )
          rhs->setUdpNumRxThreads( udp["numRxThreads"].as<unsigned>() );
      if( udp["pollSecs"] )
          rhs->setUdpPollSecs( udp["pollSecs"].as<int>() ); 
    }
    if ( node["RSSI"] )
      rhs->useRssi( node["RSSI"].as<bool>() );
    if ( node["depack"] )
    {
      const YAML::Node& depack = node["depack"];
      rhs->useDepack( true );
      if ( depack["outQueueDepth"] )
         rhs->setDepackOutQueueDepth( depack["outQueueDepth"].as<unsigned>() );
      if ( depack["ldFrameWinSize"] )
         rhs->setDepackLdFrameWinSize( depack["ldFrameWinSize"].as<unsigned>() );
      if ( depack["ldFragWinSize"] )
         rhs->setDepackLdFragWinSize( depack["ldFragWinSize"].as<unsigned>() );
    }
    if ( node["SRPMux"] )
    {
      const YAML::Node& SRPMux = node["SRPMux"];
      rhs->useSRPMux( true );
      if ( SRPMux["VirtualChannel"] )
        rhs->setSRPMuxVirtualChannel( SRPMux["VirtualChannel"].as<unsigned>() );
    }
    if ( node["TDestMux"] )
    {
      const YAML::Node& TDestMux = node["TDestMux"];
      rhs->useTDestMux( true );
      if ( TDestMux["TDEST"] )
        rhs->setTDestMuxTDEST( TDestMux["TDEST"].as<unsigned>() );
      if ( TDestMux["StripHeader"] )
        rhs->setTDestMuxStripHeader( TDestMux["StripHeader"].as<bool>() );
      if ( TDestMux["outQueueDepth"] )
        rhs->setTDestMuxOutQueueDepth( TDestMux["outQueueDepth"].as<unsigned>() );
    }

    return true;
  }
};

template<>
struct convert<NetIODev> {
  static bool decode(const Node& node, NetIODev& rhs) {
    std::string name = node["name"].as<std::string>();
    std::string ipAddr = node["ipAddr"].as<std::string>();
    rhs = INetIODev::create( name.c_str(), ipAddr.c_str() );
    INetIODev::PortBuilder bldr; 
    Field f;

    if ( node["MMIODev"] )
    {
      const YAML::Node& mmio = node["MMIODev"];
      for( unsigned i = 0; i < mmio.size(); i++ )
      {
        f    = mmio[i].as<MMIODev>();
        bldr = mmio[i].as<INetIODev::PortBuilder>();
        rhs->addAtAddress( f, bldr );
      }
    }
    if ( node["StreamDev"] )
    {
      const YAML::Node& stream = node["StreamDev"];
      for( unsigned i = 0; i < stream.size(); i++ )
      {
        f    = IField::create( stream[i]["name"].as<std::string>().c_str() );
        bldr = stream[i].as<INetIODev::PortBuilder>();
        rhs->addAtAddress( f, bldr );
      }
    }

    return true;
  }
};
}

// YAML Emitter overloads

YAML::Emitter& operator << (YAML::Emitter& out, const ScalVal_RO& s) {
    uint64_t u64;
    s->getVal( &u64 );
    out << YAML::BeginMap;
    out << YAML::Key << s->getName();
    out << YAML::Value << u64;
    out << YAML::EndMap;
    return out;
}

YAML::Emitter& operator << (YAML::Emitter& out, const Hub& h) {
    Children ch = h->getChildren();
    for( unsigned i = 0; i < ch->size(); i++ )
    {
//       out << ch[i]; 
    }
    return out;
}

#endif
