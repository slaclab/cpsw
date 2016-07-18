#include <cpsw_yaml.h>
#include <cpsw_entry.h>
#include <cpsw_hub.h>
#include <cpsw_error.h>
#include <cpsw_sval.h>
#include <cpsw_mem_dev.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_netio_dev.h>
#include <cpsw_command.h>
#include <cpsw_preproc.h>

#include <sstream>
#include <fstream>
#include <iostream>

using boost::dynamic_pointer_cast;

// Weird things happen when we do iterative lookup.
// Must be caused by how nodes are implemented and
// reference counted.
// Use RECURSIVE lookup!
#define RECURSIVE_GET_NODE
#ifndef RECURSIVE_GET_NODE
const YAML::Node getNode(const YAML::Node &node, const char *key)
{
YAML::Node n(node);

	do {
std::cout<< "looking for "<<key;
		const YAML::Node & val_node( n[key] );
		if ( val_node ) {
std::cout<< " -> found: " << val_node << "\n";
			return val_node;
		}

std::cout<< " -> not found --> looking for merge node ";

		const YAML::Node & merge_node( n["<<"] );

		// seems we have to be really careful when 
		// manipulating nodes (c++ - we don't really
		// know what's happening under the hood).
		// The 'natural' way to write things would
		// result in crashes:
		//
		//   } while ( (n = n["<<"]) );
		//
		// seems we shouldn't assign to 'n' if n["<<"] is
		// not defined.

		if ( ! merge_node ) {
std::cout<< " -> not found\n";
			return merge_node;
		}
		n = merge_node;
std::cout<< "found - IT - ";

	} while ( 1 );
}
#else
const YAML::Node getNode(const YAML::Node &node, const char *key)
{
	if ( ! node )
		return node;

	const YAML::Node &val( node[key] );

	if ( val )
		return val;

	return getNode( node["<<"], key );
}
#endif

YAML::Node CYamlSupportBase::overrideNode(const YAML::Node &node)
{
	return node;
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

void
CYamlSupportBase::dumpYamlConfig(YAML::Node &node, Path p) const
{
}

void
CYamlSupportBase::loadYamlConfig(const YAML::Node &node, Path p) const
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

template <typename T> void
CYamlTypeRegistry<T>::extractClassName(std::vector<std::string> *svec_p, const YAML::Node &n)
{
	const YAML::Node &node( getNode(n, "class") );
	if ( ! node ) {
		throw NotFoundError( std::string("property '") + std::string("class") + std::string("'") );
	} else {
		if( node.IsSequence() ) {
			for ( YAML::const_iterator it=node.begin(); it != node.end(); ++it ) {
				svec_p->push_back( it->as<std::string>() );
			}
		} else if (node.IsScalar() ) {
			svec_p->push_back( node.as<std::string>() );
		} else {
			throw  InvalidArgError( std::string("property '") + std::string("class") + std::string("'") );
		}
	}
}

void
CYamlFieldFactoryBase::addChildren(CEntryImpl &e, const YAML::Node &node, IYamlTypeRegistry<Field> *registry)
{
}

void
CYamlFieldFactoryBase::addChildren(CDevImpl &d, const YAML::Node &node, IYamlTypeRegistry<Field> *registry)
{
const YAML::Node & children( node["children"] );

	std::cout << "node size " << node.size() << "\n";

	YAML::const_iterator it = node.begin();
	while ( it != node.end() ) {
		std::cout << it->first << "\n";
		++it;
	}

	std::cout<<"adding " << children.size() << " children\n";

	if ( children ) {
		YAML::const_iterator it = children.begin();
		while ( it != children.end() ) {
			Field c = registry->makeItem( *it );
			d.addAtAddress( c, *it );
			++it;
		}
	}
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
const YAML::Node &root( root_name ? node[root_name] : node );
	/* Root node must be a Dev */
	return dynamic_pointer_cast<Dev::element_type>( getFieldRegistry()->makeItem( root ) );
}

class NoOpDeletor {
public:
	void operator()(StreamMuxBuf::Stream::element_type *obj)
	{
	}
};

static YAML::Node
loadPreprocessedYamlStream(StreamMuxBuf::Stream top_stream)
{
StreamMuxBuf         muxer;

YamlPreprocessor     preprocessor( top_stream, &muxer );

		preprocessor.process();

		std::istream top_preprocessed_stream( &muxer );

		return YAML::Load( top_preprocessed_stream );
}

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

Hub
IHub::loadYamlFile(const char *file_name, const char *root_name)
{
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name ), root_name );
}

Hub
IHub::loadYamlStream(std::istream &in, const char *root_name)
{
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYaml( in ), root_name );
}

Hub
IHub::loadYamlStream(const char *yaml, const char *root_name)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name );
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

void
IYamlSupport::dumpYamlConfig( Path p, const char *file_name )
{

shared_ptr<const EntryImpl::element_type> topi;

	if ( p->empty() ) {  // start from root
		Hub h = p->origin();
		topi = dynamic_pointer_cast<const EntryImpl::element_type>( h );
	} else {            // start from path relative to root
		Address a = CompositePathIterator( &p )->c_p_;
		topi = dynamic_pointer_cast<const EntryImpl::element_type>( a->getEntryImpl() ); 
	}

	if ( ! topi ) {
		std::cerr << "WARNING: 'top' not an EntryImpl?\n";
		return;
	}



	YAML::Node top_node;
	top_node["root"] = p->origin()->getName();
	top_node["path"] = p->toString();
	topi->dumpYamlConfig( top_node, p );


	YAML::Emitter emit;


	emit << top_node;

        if ( file_name ) {
                std::ofstream os( file_name );
                os        << emit.c_str() << "\n";
        } else {
                std::cout << emit.c_str() << "\n";
        }
}

void
IYamlSupport::loadYamlConfig( Path p, const char *file_name)
{
	YAML::Node node = YAML::Load( file_name );
	loadYamlConfig( p, node );
}

void
IYamlSupport::loadYamlConfig( Path p, const YAML::Node &node )
{
	std::string yaml_path = node["path"].as<std::string>();
	try {
		Path load_path = p->findByName( yaml_path.c_str() );
		Address a = CompositePathIterator( &load_path )->c_p_;
		shared_ptr<const EntryImpl::element_type> topi( dynamic_pointer_cast<const EntryImpl::element_type>( a->getEntryImpl() ) );
	
		if ( ! topi ) {
			std::cerr << "WARNING: 'top' not an EntryImpl?\n";
			return;
		}

		topi->loadYamlConfig( node, load_path );
	} catch (...) {

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
