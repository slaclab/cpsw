#ifndef CPSW_YAML_H
#define CPSW_YAML_H

#include <cpsw_api_builder.h>
#include <cpsw_shared_obj.h>


#include <yaml-cpp/yaml.h>

class CYamlSupportBase;
typedef shared_ptr<CYamlSupportBase> YamlSupportBase;

class CYamlSupportBase : public virtual IYamlSupportBase {
	public:
		// subclass may implement a constructor from a YAML::Node

		// every class implements this and MUST chain through its
		// superclass first (thus, derived classes can overwrite fields)
		// e.g.,
		//
		// void CYamlSupportSubclass::dumpYamlPart(YAML::Node &node)
		// {
		//    CYamlSupportBase::dumpYamlPart( node ); // chain
		//
		//    dump subclass fields into node here (may override superclass entries)
		//    node["myField"] = myvalue;
		// }
		virtual void dumpYamlPart(YAML::Node &node) const;

		// When using the 'CYamlFieldFactory' then every subclass
		// MUST define a static _getClassName() member and let it
		// return a unique value;
		// Other factories may use other ways to determine the
		// name that is entered into the registry...
		// Copy-paste this method and modify the name.
		static const char  *_getClassName() { return "YamlSupportBase"; }

		// Every subclass MUST implement the 'getClassName()' virtual
		// method (for polymorphic access to the class name).
		// Just copy-paste this one:
		virtual const char *getClassName() const { return _getClassName(); }

		// subclass MAY implement 'overrideNode()' which is executed
		// by dispatchMakeField() before the new entity is created.
		// Can be used to modify the YAML node before it is passed
		// to the constructor (create a new node from 'orig', modify
		// and return it).
		static YAML::Node overrideNode(const YAML::Node &orig);

		// insert the classname into a node;
		// e.g.,:
		//   node["class"] = getClassName();
		// or
		//   node.SetTag( getClassName() );
		virtual void setClassName(YAML::Node &node) const;

		// used by CYamlSupportBase and DevImpl to append class name and iterate
		// through children; subclass probably wants to leave this alone unless
		// you know what you are doing...
		virtual void dumpYaml(YAML::Node &) const;
};


class CDevImpl;
typedef shared_ptr<CDevImpl> DevImpl;

/* Template specializations to convert YAML nodes to CPSW entries 
 * (ByteOrder, IIntField::Mode, Field, MMIODev, etc.)
 */

// Lookup a node iterating through merge tags
const YAML::Node getNode(const YAML::Node &node, const char *fld);

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
		if ( ! node.IsSequence() ) {
			return false;
		}

		MutableEnum e = IMutableEnum::create( node );

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


template <typename T> class IYamlFactoryBase;

template <typename T> class IYamlTypeRegistry {
public:

	virtual T    makeItem(const YAML::Node &) = 0;

	virtual void extractClassName(std::vector<std::string> *svec_p, const YAML::Node &)  = 0;

	virtual void addFactory(const char *className, IYamlFactoryBase<T> *f) = 0;
	virtual void delFactory(const char *className)                         = 0;

	virtual void dumpClasses()                                             = 0;
};

class IRegistry {
public:
	virtual void  addItem(const char *name, void *item) = 0;
	virtual void  delItem(const char *name)             = 0;
	virtual void *getItem(const char *name)             = 0;

	virtual void  dumpItems()                           = 0;

	virtual      ~IRegistry() {}

	static IRegistry *create();
};

template <typename T> class IYamlFactoryBase {
private:
	IYamlTypeRegistry<T> *registry_	;
	const char           *className_;
public:
	IYamlFactoryBase(const char *className, IYamlTypeRegistry<T> *r)
	: registry_(r),
	  className_(className)
	{
		r->addFactory( className, this );
	}

	virtual T makeItem(const YAML::Node &node, IYamlTypeRegistry<T> *) = 0;

	virtual ~IYamlFactoryBase()
	{
		registry_->delFactory( className_ );
	}
};

template <typename T> class CYamlTypeRegistry : public IYamlTypeRegistry<T> {
private:

	IRegistry *registry_;

	CYamlTypeRegistry(const CYamlTypeRegistry&);
	CYamlTypeRegistry operator=(const CYamlTypeRegistry &);

public:
	CYamlTypeRegistry()
	: registry_( IRegistry::create() )
	{
	}

	virtual void addFactory(const char *className, IYamlFactoryBase<T> *f)
	{
		registry_->addItem(className, f);
	}

	virtual void delFactory(const char *className)
	{
		registry_->delItem(className);
	}

	virtual void extractClassName(std::vector<std::string> *svec_p, const YAML::Node &n);

	virtual T makeItem(const YAML::Node &n)
	{
	std::vector<std::string> str_vec;
	std::string str_no_factory = "";
		extractClassName( &str_vec, n  );
		if ( str_vec.size() == 0 )
			throw NotFoundError("CYamlTypeRegistry: node w/o class info");

		for( std::vector<std::string>::iterator it = str_vec.begin(); it != str_vec.end(); ++it ) {
			IYamlFactoryBase<T> *factory = static_cast<IYamlFactoryBase<T> *>( registry_->getItem( it->c_str() ) );
			if ( factory )
				return factory->makeItem( n, this );
			str_no_factory = str_no_factory + (*it) + ", ";
		}

		throw NotFoundError(std::string("No factory for '") + str_no_factory + std::string("' found"));
	}

	virtual void dumpClasses()
	{
		registry_->dumpItems();
	}

	virtual ~CYamlTypeRegistry()
	{
		delete registry_;
	}
};

class CYamlFieldFactoryBase : public IYamlFactoryBase<Field> {
	public:
		static IYamlTypeRegistry<Field> *getFieldRegistry();
	protected:
		CYamlFieldFactoryBase(const char *typeLabel)
		: IYamlFactoryBase<Field>( typeLabel, getFieldRegistry() )
		{
		}

		virtual void addChildren(CEntryImpl &, const YAML::Node &, IYamlTypeRegistry<Field> *);
		virtual void addChildren(CDevImpl &,   const YAML::Node &, IYamlTypeRegistry<Field> *);


	public:
		static Dev dispatchMakeField(const YAML::Node &node, const char *root_name);

		static YAML::Node loadPreprocessedYaml(std::istream &);
		static YAML::Node loadPreprocessedYaml(const char *char_stream);
		static YAML::Node loadPreprocessedYamlFile(const char *file_name);

		static void dumpClasses() { getFieldRegistry()->dumpClasses(); }
};


template <typename T> class CYamlFieldFactory : public CYamlFieldFactoryBase {
public:
	CYamlFieldFactory()
	: CYamlFieldFactoryBase(T::element_type::_getClassName())
	{
	}

	virtual Field makeItem(const YAML::Node & node, IYamlTypeRegistry<Field> *registry)
	{
	const YAML::Node &n( T::element_type::overrideNode(node) );
		T fld( CShObj::create<T>(n) );
		addChildren( *fld, n, registry );
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

// For adding YAML support to a subclass of CYamlSupportBase:
//
//  - add a constructor that takes a YAML::Node & argument  (YAML -> c++ class)
//    The factory's makeItem() then uses this constructor. Alternatively,
//    makeItem() may use another constructor and build the item using
//    information from the node.
//  - add a virtual 'void dumpYamlPart(YAML::Node&)' member (c++ class -> YAML)
//    This method MUST chain through the corresponding superclass member.
//  - copy/paste virtual 'const char *getClassName()' method (from CEntryImpl)
//  - add a 'static const char * _getClassName()' member and return a unique name
//  - expand 'DECLARE_YAML_FIELD_FACTORY( shared_pointer_type )' macro ONCE from code which
//    is guaranteed to be linked by the application (can be tricky when using static
//    linking). Built-in classes do this in 'cpsw_yaml.cc'.
//

#define DECLARE_YAML_FIELD_FACTORY(FieldType) CYamlFieldFactory<FieldType> FieldType##factory_


#endif
