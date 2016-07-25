#include <sstream>
#include <fstream>
#include <iostream>

#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <cpsw_yaml.h>

#ifndef NO_YAML_SUPPORT

#include <cpsw_entry.h>
#include <cpsw_hub.h>
#include <cpsw_error.h>
#include <cpsw_sval.h>
#include <cpsw_mem_dev.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_netio_dev.h>
#include <cpsw_command.h>
#include <cpsw_preproc.h>


using boost::dynamic_pointer_cast;

// Weird things happen when we do iterative lookup.
// Must be caused by how nodes are implemented and
// reference counted.
// Use RECURSIVE lookup!
YAML::PNode
YAML::PNode::lookup(const char *key) const
{
	if ( ! key )
		return *this;

	PNode val(this, key);

	if ( val )
		return val;

	PNode merge( this, "<<" );
	if ( merge )
		return merge.lookup(key);

	return merge;
}

void pushNode(YAML::Node &node, const char *fld, const YAML::Node &child)
{
	if ( fld )
		node[fld].push_back( child );
	else
		node.push_back( child );
}


void CYamlSupportBase::overrideNode(YamlState &node)
{
}

void
CYamlSupportBase::setClassName(YAML::Node &node) const
{
	node["class"] = getClassName();
}


void
CYamlSupportBase::dumpYaml(YAML::Node &node) const
{
	setClassName( node );

	dumpYamlPart( node );
}

void
CYamlSupportBase::dumpYamlPart(YAML::Node &node) const
{
}

class RegistryImpl : public IRegistry {
	public:

	struct StrCmp {
		bool operator () (const char *a, const char *b) const {
			return ::strcmp(a,b) < 0 ? true : false;
		}
	};

	typedef std::map <const char *, void *, StrCmp> Map;
	typedef std::pair<const char *, void *>         Member;

private:
	Map map_;

public:
	virtual void  addItem(const char *name, void *item)
	{
	std::pair< Map::iterator, bool > ret = map_.insert( Member(name, item) );
	if ( ! ret.second )
		throw DuplicateNameError( name );
	}

	virtual void  delItem(const char *name)
	{
		map_.erase( name );
	}

	virtual void *getItem(const char *name)
	{
		return map_[name];
	}

	virtual void  dumpItems()
	{
	Map::iterator it( map_.begin() );
	Map::iterator ie( map_.end()   );
		while ( it != ie ) {
			std::cout << it->first << "\n";
			++it;
		}
	}

};

IRegistry *
IRegistry::create()
{
	return new RegistryImpl();
}

template <typename T> bool
CYamlTypeRegistry<T>::extractInstantiate(YamlState &node)
{
bool        instantiate    = true;

	readNode( node, "instantiate", &instantiate );

	return instantiate;
}

template <typename T> void
CYamlTypeRegistry<T>::extractClassName(std::vector<std::string> *svec_p, YamlState &node)
{
	const YAML::PNode &class_node( node.lookup("class") );
	if ( ! class_node ) {
		throw NotFoundError( std::string("property '") + std::string("class") + std::string("'") );
	} else {
		if( class_node.IsSequence() ) {
			for ( YAML::const_iterator it=class_node.begin(); it != class_node.end(); ++it ) {
				svec_p->push_back( it->as<std::string>() );
			}
		} else if (class_node.IsScalar() ) {
			svec_p->push_back( class_node.as<std::string>() );
		} else {
			throw  InvalidArgError( std::string("property '") + std::string("class") + std::string("'") );
		}
	}
}

void
CYamlFieldFactoryBase::addChildren(CEntryImpl &e, YamlState &node, IYamlTypeRegistry<Field> *registry)
{
}

void
CYamlFieldFactoryBase::addChildren(CDevImpl &d, YamlState &node, IYamlTypeRegistry<Field> *registry)
{
const YAML::PNode & children( node.lookup("children") );

	std::cout << "node size " << node.size() << "\n";

	YAML::const_iterator it = node.begin();
	while ( it != node.end() ) {
		std::cout << it->first << "\n";
		++it;
	}


	if ( children ) {
		unsigned i;
		for ( i=0; i<children.size(); i++ ) {
			const YAML::PNode child( &children, i );
			Field c = registry->makeItem( child );
			if ( c ) {
				// if 'instantiate' is 'false' then
				// makeItem() returns a NULL pointer
				d.addAtAddress( c, child );
			}
		}
	}
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
YamlState &root( root_name ? node[root_name] : node );
	/* Root node must be a Dev */
	return dynamic_pointer_cast<Dev::element_type>( getFieldRegistry()->makeItem( root ) );
}

static YAML::Node
loadPreprocessedYamlStream(StreamMuxBuf::Stream top_stream)
{
StreamMuxBuf         muxer;

YamlPreprocessor     preprocessor( top_stream, &muxer );

		preprocessor.process();

		std::istream top_preprocessed_stream( &muxer );

		return YAML::Load( top_preprocessed_stream );
}

class NoOpDeletor {
public:
	void operator()(StreamMuxBuf::Stream::element_type *obj)
	{
	}
};

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(std::istream &top)
{
StreamMuxBuf         muxer;
StreamMuxBuf::Stream top_stream( &top, NoOpDeletor() );
	return loadPreprocessedYamlStream( top_stream );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(const char *yaml)
{
std::string       str( yaml );
std::stringstream sstrm( str );

	return loadPreprocessedYaml( sstrm );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYamlFile(const char *file_name)
{
	return loadPreprocessedYamlStream( StreamMuxBuf::mkstrm( file_name ) );
}

void
IYamlSupport::dumpYamlFile(Entry top, const char *file_name, const char *root_name)
{
shared_ptr<const EntryImpl::element_type> topi( dynamic_pointer_cast<const EntryImpl::element_type>(top) );

	if ( ! topi ) {
		std::cerr << "WARNING: 'top' not an EntryImpl?\n";
		return;
	}

	YAML::Node top_node;
	topi->dumpYaml( top_node );


	YAML::Emitter emit;

	if ( root_name ) {
		YAML::Node root_node;
		root_node[root_name]=top_node;
		emit << root_node;
	} else {
		emit << top_node;
	}

	if ( file_name ) {
		std::ofstream os( file_name );
		os        << emit.c_str() << "\n";
	} else {
		std::cout << emit.c_str() << "\n";
	}
}

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

IYamlTypeRegistry<Field> *
CYamlFieldFactoryBase::getFieldRegistry()
{
static CYamlTypeRegistry<Field> the_registry_;
	return &the_registry_;
}

DECLARE_YAML_FIELD_FACTORY(EntryImpl);
DECLARE_YAML_FIELD_FACTORY(DevImpl);
DECLARE_YAML_FIELD_FACTORY(IntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(MemDevImpl);
DECLARE_YAML_FIELD_FACTORY(MMIODevImpl);
DECLARE_YAML_FIELD_FACTORY(NetIODevImpl);
DECLARE_YAML_FIELD_FACTORY(SequenceCommandImpl);

#endif

Hub
IHub::loadYamlFile(const char *file_name, const char *root_name)
{
#ifdef NO_YAML_SUPPORT
	throw NoYAMLSupportError();
#else
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name ), root_name );
#endif
}

Hub
IHub::loadYamlStream(std::istream &in, const char *root_name)
{
#ifdef NO_YAML_SUPPORT
	throw NoYAMLSupportError();
#else
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYaml( in ), root_name );
#endif
}

Hub
IHub::loadYamlStream(const char *yaml, const char *root_name)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name );
}

#ifdef NO_YAML_SUPPORT
void
IYamlSupport::dumpYamlFile(Entry top, const char *file_name, const char *root_name)
{
	throw NoYAMLSupportError();
}
#endif
