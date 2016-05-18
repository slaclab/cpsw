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
struct convert<INetIODev::ProtocolVersion> {
  static bool decode(const Node& node, INetIODev::ProtocolVersion& rhs) {
    if (!node.IsScalar())
      return false;

    std::string str = node.Scalar();

    if( str.compare( "SRP_UDP_V1" ) == 0 )
      rhs = INetIODev::SRP_UDP_V1;
    else if (str.compare( "SRP_UDP_V2" ) == 0 ) 
      rhs = INetIODev::SRP_UDP_V2;
    else if (str.compare( "SRP_UDP_V3" ) == 0 ) 
      rhs = INetIODev::SRP_UDP_V3;
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

    std::string name;
    int val;
    IIntField::Mode mode;
    
    name = node["name"].as<std::string>();
    bldr->name( name.c_str() );

    if( node["sizeBits"] ) {
        val = node["sizeBits"].as<int>();
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
    std::vector<std::string> fields;
    std::vector<uint64_t> values;

    name = "C_";
    name = name + node["name"].as<std::string>();

    if( node["sequence"] ) {
        const YAML::Node& seq = node["sequence"];
        for( unsigned i = 0; i < seq.size(); i++ )
        {
            fields.push_back( seq[i]["field"].as<std::string>() );
            values.push_back( seq[i]["value"].as<uint64_t>() );
        }
    }
    else {
        return false;
    }


    rhs = ISequenceCommand::create( name.c_str(), fields, values );

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

    // This device contains iField
    if ( node["IntField"] )
    {
      const YAML::Node& iField = node["IntField"];
      for( unsigned i = 0; i < iField.size(); i++ )
      {
        uint64_t offset = iField[i]["offset"].as<uint64_t>();
        int nelms;
        if( iField[i]["nelms"] )
          nelms = iField[i]["nelms"].as<int>();
        else
          nelms = 1;
//        int nelms   = iField[i]["nelms"] ? 
//                      iField[i]["nelms"].as<int>() : 1;
        uint64_t stride = iField[i]["stride"] ? 
                          iField[i]["stride"].as<uint64_t>() : IMMIODev::STRIDE_AUTO;
        ByteOrder byteOrder = iField[i]["ByteOrder"] ? iField[i]["ByteOrder"].as<ByteOrder>() : UNKNOWN;
        std::string desc = iField[i]["description"].as<std::string>();
        IField::Cacheable cacheable = iField[i]["cacheable"] ? \
                       iField[i]["cacheable"].as<IField::Cacheable>() : \
                       IField::UNKNOWN_CACHEABLE;
        f = iField[i].as<IntField>();
        f->setDescription( desc );
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
//    const YAML::Node& mmio = node["MMIODev"];
    if ( node["MMIODev"] )
    {
      const YAML::Node& mmio = node["MMIODev"];
      for( unsigned i = 0; i < mmio.size(); i++ )
      {
/*change address to offset */
        uint64_t offset = mmio[i]["offset"].as<uint64_t>();
        int nelms   = mmio[i]["nelms"] ? mmio[i]["nelms"].as<int>() : 1;
        f = mmio[i].as<MMIODev>();
        rhs->addAtAddress( f, offset, nelms );
      }
    }
    return true;
  }
};

/*
template<>
struct convert<NetIODev> {
  static bool decode(const Node& node, NetIODev& rhs) {
    MMIODev mmio;
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
*/
}

#endif
