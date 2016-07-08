#ifndef CPSW_YAML_H
#define CPSW_YAML_H

#include <yaml-cpp/yaml.h>
#include <cpsw_api_builder.h>
#include <cpsw_shared_obj.h>

class CDevImpl;
typedef shared_ptr<CDevImpl> DevImpl;

/* Template specializations to convert YAML nodes to CPSW entries 
 * (ByteOrder, IIntField::Mode, Field, MMIODev, etc.)
 */

// Lookup a node iterating through merge tags
//#define getNode(n,f) (n)[(f)]

#ifndef getNode
const YAML::Node getNode(const YAML::Node &node, const char *fld);
#endif

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

  static Node encode(const ByteOrder &rhs) {
    Node node;
    if ( LE == rhs )
      node = "LE";
    else if ( BE == rhs )
      node = "BE";
    else
      node = "UNKNOWN";
    return node;
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
  static Node encode(const IField::Cacheable& rhs) {
	Node node;

	switch ( rhs ) {
		case IField::NOT_CACHEABLE: node = "NOT_CACHEABLE"; break;
		case IField::WT_CACHEABLE:  node = "WT_CACHEABLE";  break;
		case IField::WB_CACHEABLE:  node = "WB_CACHEABLE";  break;
		default:                    node = "UNKNOWN_CACHEABLE"; break;
	}

    return node;
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
  static Node encode(const IIntField::Mode &rhs) {
    Node node;
    switch ( rhs ) {
      case IIntField::RO: node = "RO"; break;
      case IIntField::RW: node = "RW"; break;
      case IIntField::WO: node = "WO"; break;
	  default:
	  break;
    }
	return node;
  }
};

template<>
struct convert<MutableEnum> {
	static bool decode(const Node &node, MutableEnum &rhs)
	{
		if ( ! node.IsSequence() )
			return false;

		MutableEnum e = IMutableEnum::create();

		for ( YAML::const_iterator it(node.begin()); it != node.end(); ++it ) {
			const YAML::Node &nam( getNode( *it, "name"  ) );
			const YAML::Node &val( getNode( *it, "value" ) );
			if ( ! nam || ! val )
				return false;
			e->add( nam.as<std::string>().c_str(), val.as<uint64_t>() );
		}
		rhs = e;
		return true;
	}
};

#if 0
template<>
struct convert<IIntField::Builder> {
	static bool decode(const Node &node, IIntField::Builder &rhs)
	{
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
		bldr->setEnum( node["enums"].as<MutableEnum>() );
    }

	rhs = bldr;

	return true;
	}
};

template<>
struct convert<IntField> {
  static bool decode(const Node& node, IntField& rhs) {
//    if(!node.IsMap() || node.size() >= 3) {
//      return false;
//    }
	IIntField::Builder bldr( node.as<IIntField::Builder>() );

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


    //name = "C_";
    //name = name + node["name"].as<std::string>();
    name = node["name"].as<std::string>();

    if( node["sequence"] ) {
        const YAML::Node& seq = node["sequence"];
        for( unsigned i = 0; i < seq.size(); i++ )
        {
            fields.push_back( seq[i]["entry"].as<std::string>() );
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
#endif

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

  static Node encode(const INetIODev::ProtocolVersion &rhs) {
    Node node;
    switch( rhs ) {
      case INetIODev::SRP_UDP_V1: node = "SRP_UDP_V1"; break;
      case INetIODev::SRP_UDP_V2: node = "SRP_UDP_V2"; break;
      case INetIODev::SRP_UDP_V3: node = "SRP_UDP_V3"; break;
      default: node = "SRP_UDP_NONE"; break;
	}
    return node;
  }

};

template<>
struct convert<INetIODev::PortBuilder> {
  static bool decode(const Node& node, INetIODev::PortBuilder& rhs) {
    rhs = INetIODev::createPortBuilder(); 
	{
	const YAML::Node &nn( getNode(node, "SRP") );
    if( nn )
    {
	  {
	  const YAML::Node &n( getNode(nn, "ProtocolVersion") );
      if( n )
          rhs->setSRPVersion( n.as<INetIODev::ProtocolVersion>() ); 
	  }
	  {
	  const YAML::Node &n( getNode(nn, "TimeoutUS") );
      if( n )
          rhs->setSRPTimeoutUS( n.as<uint64_t>() ); 
	  }
	  {
	  const YAML::Node &n( getNode(nn, "DynTimeout") );
      if( n )
          rhs->useSRPDynTimeout( n.as<bool>() ); 
	  }
	  {
	  const YAML::Node &n( getNode(nn, "RetryCount") );
      if( n )
          rhs->setSRPRetryCount( n.as<unsigned>() ); 
	  }
    }
	}
	{
	const YAML::Node &nn( getNode(node, "udp") );
    if ( nn )
    {
	  {
	  const YAML::Node &n( getNode(nn, "port") );
      if( n )
          rhs->setUdpPort( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "outQueueDepth") );
      if( n )
          rhs->setUdpOutQueueDepth( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "numRxThreads") );
      if( n )
          rhs->setUdpNumRxThreads( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "pollSecs") );
      if( n )
          rhs->setUdpPollSecs( n.as<int>() ); 
	  }
    }
	}
	{
	const YAML::Node &nn( getNode(node, "RSSI" ) );
	  if ( nn )
          rhs->useRssi( nn.as<bool>() );
	}
	{
	const YAML::Node &nn( getNode(node, "depack") );
	if (nn )
    {
      rhs->useDepack( true );
	  {
	  const YAML::Node &n( getNode(nn, "outQueueDepth") );
      if( n )
         rhs->setDepackOutQueueDepth( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "ldFrameWinSize") );
      if( n )
         rhs->setDepackLdFrameWinSize( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "ldFragWinSize") );
      if( n )
         rhs->setDepackLdFragWinSize( n.as<unsigned>() );
	  }
    }
	}
	{
	const YAML::Node &nn( getNode(node, "SRPMux") );
	if (nn )
    {
      rhs->useSRPMux( true );
	  {
	  const YAML::Node &n( getNode(nn, "VirtualChannel") );
      if( n )
          rhs->setSRPMuxVirtualChannel( n.as<unsigned>() );
	  }
    }
	}
	{
	const YAML::Node &nn( getNode(node, "TDestMux") );
	if (nn )
    {
      rhs->useTDestMux( true );
	  {
	  const YAML::Node &n( getNode(nn, "TDEST") );
      if( n )
        rhs->setTDestMuxTDEST( n.as<unsigned>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "StripHeader") );
      if( n )
        rhs->setTDestMuxStripHeader( n.as<bool>() );
	  }
	  {
	  const YAML::Node &n( getNode(nn, "outQueueDepth") );
      if( n )
        rhs->setTDestMuxOutQueueDepth( n.as<unsigned>() );
	  }
    }
	}

    return true;
  }
};

#if 0
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
#endif
}

// YAML Emitter overloads

YAML::Emitter& operator << (YAML::Emitter& out, const ScalVal_RO& s);
YAML::Emitter& operator << (YAML::Emitter& out, const Hub& h);

class CYamlFactoryBaseImpl {
	protected:
		class TypeRegistry;

		static TypeRegistry *getRegistry();

		CYamlFactoryBaseImpl(const char *typeLabel);

		virtual EntryImpl makeField(const YAML::Node &) = 0;

		virtual void addChildren(CEntryImpl &, const YAML::Node &);
		virtual void addChildren(CDevImpl &, const YAML::Node &);

		static  Field dispatchMakeField(const YAML::Node &);

	public:
		static Dev loadYamlFile(const char *file_name);
		static Dev loadYamlStream(std::istream &yaml);
		// convenience wrapper
		static Dev loadYamlStream(const char *yaml);

		static void dumpTypes();
};

template <typename T> class CYamlFieldFactory : public CYamlFactoryBaseImpl {
public:
	CYamlFieldFactory()
	: CYamlFactoryBaseImpl(T::element_type::className_)
	{
	}

	virtual EntryImpl makeField(const YAML::Node & node)
	{
	const YAML::Node &n( T::element_type::overrideNode(node) );
		T fld( CShObj::create<T>(n) );
		addChildren( *fld, n );
		return fld;
	}

};

template <typename T> static void mustReadNode(const YAML::Node &node, const char *fld, T *val)
{
	const YAML::Node &n( getNode(node, fld) );
	if ( ! n ) {
		throw NotFoundError( std::string("property '") + std::string(fld) + std::string("'") );
	} else {
		*val = n.as<T>();
	}
}

template <typename T> static void readNode(const YAML::Node &node, const char *fld, T *val)
{
	const YAML::Node &n( getNode(node, fld) );
	if ( n ) {
		*val = n.as<T>();
	}
}


#define DECLARE_YAML_FACTORY(FieldType) CYamlFieldFactory<FieldType> FieldType##factory_

#endif
