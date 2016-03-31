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
    else if (str.compare( "SRP_UDP_V2" ) == 0 ) 
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
struct convert<INoSsiDev::ProtocolVersion> {
  static bool decode(const Node& node, INoSsiDev::ProtocolVersion& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if( str.compare( "SRP_UDP_V1" ) == 0 )
      rhs = INoSsiDev::SRP_UDP_V1;
    else if (str.compare( "SRP_UDP_V2" ) == 0 ) 
      rhs = INoSsiDev::SRP_UDP_V2;
    else
      return false;

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
      rhs = IIntField::WO;
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

    std::string name;
    int val;
    IIntField::Mode mode;
    
    name = node["name"].as<std::string>();
    bldr->name( name.c_str() );

    if( node["size"] ) {
        val = node["size"].as<int>();
        bldr->sizeBits( val );
    }
    if( node["isSigned"] ) {
        val = node["isSigned"].as<int>();
        bldr->isSigned( (bool) val );
    }
    if( node["lsBit"] ) {
        val = node["lsBit"].as<int>();
        bldr->lsBit( val );
    }
    if( node["mode"] ) {
        mode = node["mode"].as<IIntField::Mode>();
        bldr->mode( mode );
    }
    if( node["wordSwap"] ) {
        val = node["wordSwap"].as<int>();
        bldr->wordSwap( val );
    }

    rhs = bldr->build();

    return true;
  }
};

template<>
struct convert<MMIODev> {
  static bool decode(const Node& node, MMIODev& rhs) {

    std::string name = node["name"].as<std::string>();
    int size = node["size"].as<int>();
    rhs = IMMIODev::create( name.c_str(), size );
    Field f;

    // This device contains registers
    if ( node["registers"] )
    {
      const YAML::Node& registers = node["registers"];
      for( unsigned i = 0; i < registers.size(); i++ )
      {
        int address = registers[i]["address"].as<int>();
        int nelms   = registers[i]["nelms"] ? \
                        registers[i]["nelms"].as<int>() : \
                        1;
        uint64_t stride =registers[i]["stride"] ? 
                       registers[i]["stride"].as<uint64_t>() : 
                       IMMIODev::STRIDE_AUTO;
        ByteOrder byteOrder = registers[i]["ByteOrder"] ? registers[i]["ByteOrder"].as<ByteOrder>() : UNKNOWN;
        std::string desc = registers[i]["description"].as<std::string>();
        IField::Cacheable cacheable = registers[i]["cacheable"] ? \
                       registers[i]["cacheable"].as<IField::Cacheable>() : \
                       IField::UNKNOWN_CACHEABLE;
        f = registers[i].as<IntField>();
        f->setDescription( desc );
        rhs->addAtAddress( f, address, nelms, stride, byteOrder );
      }
    }

    // This device contains other peripherals
    if ( node["peripherals"] )
    {
      const YAML::Node& peripherals = node["peripherals"];
      for( unsigned i = 0; i < peripherals.size(); i++ )
      {
        int address = peripherals[i]["address"].as<int>();
        int nelms   = peripherals[i]["nelms"] ? peripherals[i]["nelms"].as<int>() : 1;
        f = peripherals[i].as<MMIODev>();
        rhs->addAtAddress( f, address, nelms );
      }
    }
    return true;
  }
};

}

#endif
