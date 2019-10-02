 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <sstream>
#include <fstream>
#include <iostream>

#include <cpsw_compat.h>

#include <string.h>

#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <cpsw_yaml.h>

#include <cpsw_entry.h>
#include <cpsw_hub.h>
#include <cpsw_error.h>
#include <cpsw_sval.h>
#include <cpsw_const_sval.h>
#include <cpsw_mem_dev.h>
#include <cpsw_null_dev.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_netio_dev.h>
#include <cpsw_command.h>
#include <cpsw_preproc.h>
#include <cpsw_yaml_merge.h>

#include <dlfcn.h>

using cpsw::dynamic_pointer_cast;
using cpsw::unordered_set;

using YAML::PNode;
using YAML::Node;
using std::cout;

#undef  CPSW_YAML_DEBUG

typedef unordered_set<std::string> StrSet;

// PNode Implementation

void
PNode::dump() const
{
	printf( "PNode key: %s, p: %p\n" , key_, parent_ );
}


#if 0
// assign a new Node to this PNode while preserving parent/child/key
// ('merge' operation)
PNode &
PNode::operator=(const Node &orig_node)
{
	// YAML::Node objects seem to behave somehow like references.
	// I.e., when we
	//
	//   YAML::Node nodeA = nodeB;
	//
	//              nodeA = nodeC;
	//
	// then the 'contents' of what node C refers to
	// seem to be assigned to what node A refers to.
	//
	// almost like the following operation:
	//
	//   assign(NODE **p, NODE *rhs) {
	//	   if ( *p )
	//        **p = *rhs; // if pointer already in use copy contents
	//     else
	//        *p  = rhs;  // if pointer NULL then assign pointer
	//   }
	//
	// The above is not what we really want. It seems that 'reset()'
	// just transfers 'references' (rather than contents)
	//
	if ( this != &orig_node ) {
		static_cast<YAML::Node &>(*this).reset( static_cast<const YAML::Node &>(orig_node) );
	}
	return *this;
}
#endif

PNode::PNode( const PNode *parent, const char *key, const Node &node)
: Node   ( node   ),
  parent_( parent ),
  key_   ( key    )
{
}

std::string
PNode::toString() const
{
	if ( parent_ )
		return parent_->toString() + "/" + key_;
	return std::string("/");
}

// somewhere between 0.5.1 and 0.5.3 two 'flavors' of undefined nodes
// were introduced (without the user being able to tell the difference :-()
// 'valid' nodes which are 'undefined' and 'invalid' nodes (which have no
// memory behind them). Both of these return 'IsDefined()' ==> false
// or 'operator!()' ==> true.
// Breaking 0.5.1, 'undefined' nodes now may throw exceptions when
// operation are attempted and the node is 'invalid'. 'invalid' nodes
// can never be made 'valid'. Thus, this case must be caught in
// the constructor...
static Node fixInvalidNode(const Node &n)
{
	// if operator!() returns 'true' then make sure
	// a proper 'undefined' node is used.
	return n ? n : Node( YAML::NodeType::Undefined );
}

// Construct a PNode by map lookup while remembering
// the parent.
PNode::PNode( const PNode *parent, const char *key )
: Node   ( fixInvalidNode((*parent)[key]) ),
  parent_( parent ),
  key_   ( key    )
{
}

// Construct a PNode by sequence lookup while remembering
// the parent.
PNode::PNode( const PNode *parent, unsigned index )
: Node( fixInvalidNode( (*parent)[index] ) ),
  parent_( parent   ),
  key_   ( "<none>" )
{
}

PNode::~PNode()
{
}


#ifdef CPSW_YAML_DEBUG
void
isd(PNode *pn)
{
if ( pn->IsDefined() )
	cout << *pn << "\n";
else
	cout << "PN UNDEFINED?\n";
}


static void pp(const PNode *p)
{
	if ( p ) {
		pp(p->getParent());
		fprintf(stderr,"/%s", p->getName());
	}
}
#endif

PNode
PNode::lookup(const char *key) const
{
PNode node(this, key);

	return node;
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
	// do nothing
}

void
CYamlSupportBase::setClassName(YAML::Node &node) const
{
	node[YAML_KEY_class] = getClassName();
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

	typedef std::map<std::string,void*>   Map;
	typedef std::pair<std::string, void*> Member;
	typedef StrSet                        Set;

private:
	Map  map_;
    Set  failed_;
	bool dynLoad_;

public:
    RegistryImpl(bool dynLoad)
	: dynLoad_(dynLoad)
	{
	}

	~RegistryImpl()
	{
#ifdef CPSW_YAML_DEBUG
		fprintf(stderr,"Destroying a registry\n");
#endif
	}

	virtual void  addItem(const char *name, void *item)
	{
	std::pair< Map::iterator, bool > ret = map_.insert( Member(name, item) );
		if ( ! ret.second ) {
			throw DuplicateNameError( name );
		}
	}

	virtual void  delItem(const char *name)
	{
#ifdef CPSW_YAML_DEBUG
		fprintf(stderr,"Deleting a registry item: %s\n", name);
#endif
		map_.erase( name );
	}

	virtual void *getItem(const char *name)
	{
	void                *found = 0;
	Map::const_iterator  it;

		// operator [] inserts into the map if the key is not found !

		if ( (it = map_.find( name )) != map_.end() )
			found = it->second;

		if ( found || ! dynLoad_ || failed_.count( name ) )
			return found;

		// should try dynamic loading and has not failed yet

		std::string nam = std::string(name) + std::string(".so");

		if ( dlopen( nam.c_str(), RTLD_LAZY | RTLD_GLOBAL | RTLD_NOLOAD ) ) {
			// This object is already loaded but did not register
			// 'name' ??
			std::cerr << "Warning: '" << nam <<"' already loaded but it didn't register '" << name << "'!\n";
			goto bail;
		}

		// Now try for real

		if ( ! dlopen( nam.c_str(), RTLD_LAZY | RTLD_GLOBAL ) ) {
			std::cerr << "Warning: unable to load '" << nam <<"': " << dlerror() << "\n";
			goto bail;
		}

		// and try again
		if ( (it = map_.find( name )) != map_.end() )
			found = it->second;

		if ( ! found ) {
			std::cerr << "Warning: '" << nam <<"' loaded successfully but it didn't register '" << name << "'!\n";
			goto bail;
		}

	bail:
		if ( ! found ) {
			failed_.insert( name );
		}
		return found;
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
IRegistry::create(bool dynLoad)
{
	return new RegistryImpl(dynLoad);
}

template <typename T> bool
CYamlTypeRegistry<T>::extractInstantiate(YamlState &node)
{
bool        instantiate    = true;

	readNode( node, YAML_KEY_instantiate, &instantiate );

	return instantiate;
}

template <typename T> void
CYamlTypeRegistry<T>::extractClassName(std::vector<std::string> *svec_p, YamlState &node)
{
	const YAML::PNode &class_node( node.lookup(YAML_KEY_class) );
	if ( ! class_node ) {
		throw   NotFoundError( std::string("No property '")
			  + std::string(YAML_KEY_class)
			  + std::string("' in: ") + node.toString()
			    );
	} else {
		if( class_node.IsSequence() ) {
			for ( YAML::const_iterator it=class_node.begin(); it != class_node.end(); ++it ) {
				svec_p->push_back( it->as<std::string>() );
			}
		} else if (class_node.IsScalar() ) {
			svec_p->push_back( class_node.as<std::string>() );
		} else {
			throw   InvalidArgError( std::string("property '")
				  + std::string(YAML_KEY_class)
				  + std::string("' in: ")
				  + node.toString()
				  + std::string(" is not a Sequence nor Scalar")
				    );
		}
	}
}

void
CYamlFieldFactoryBase::addChildren(CEntryImpl &e, YamlState &node)
{
	// nothing to do for 'leaf' entries
}

class AddChildrenVisitor {
private:
	CDevImpl                          *d_;
	IYamlFactoryBase<Field>::Registry  registry_;


	// record children that were not created because 'instantiated' is false
	// we need to remember this when going through merge keys upstream
	// (which may contain the same child marked as instantiated = true).
	StrSet                    not_instantiated_;

public:
	AddChildrenVisitor(CDevImpl *d, IYamlFactoryBase<Field>::Registry registry)
	: d_(d),
	  registry_(registry)
	{
	}

	virtual bool visit(const PNode *pnode)
	{

		YAML::const_iterator it ( pnode->begin() );
		YAML::const_iterator ite( pnode->end()   );

		while ( it != ite ) {
			const std::string &k = it->first.as<std::string>();
			const YAML::Node   v = it->second;

			// skip merge node! But remember it and follow downstream
			// after all other children are handled.
			if ( 0 == k.compare( cpsw::YAML_MERGE_KEY_PATTERN ) ) {
				throw ConfigurationError("YAML still contains merge key!");
			} else {
				if ( ! d_->getChild( k.c_str() ) && 0 == not_instantiated_.count( k ) ) {
					const YAML::PNode child( pnode, k.c_str(), v );
#ifdef CPSW_YAML_DEBUG
					fprintf(stderr,"AddChildrenVisitor: trying to make child %s\n", k.c_str());
#endif
					Field c = registry_->makeItem( child );
					if ( c ) {
						// if 'instantiate' is 'false' then
						// makeItem() returns a NULL pointer
#ifdef CPSW_YAML_DEBUG
						fprintf(stderr,"AddChildrenVisitor: adding child %s\n", k.c_str());
#endif

						const YAML::PNode & child_address( child.lookup(YAML_KEY_at) );
						if ( child_address ) {
							d_->addAtAddress( c, child_address );
						} else {
							not_instantiated_.insert( k );
							std::string errmsg =   std::string("Child '")
								+ child.toString()
								+ std::string("' found but missing '" YAML_KEY_at "' key");
							throw InvalidArgError(errmsg);
						}
					} else {
#ifdef CPSW_YAML_DEBUG
						fprintf(stderr,"AddChildrenVisitor: adding child %s to 'not_instantiated' database\n", k.c_str());
#endif
						not_instantiated_.insert( k );
					}
				} else {
#ifdef CPSW_YAML_DEBUG
					fprintf(stderr,"AddChildrenVisitor: %s not instantiated\n", k.c_str());
#endif
				}
			}
			++it;
		}
		return false;
	}
};

void
CYamlFieldFactoryBase::addChildren(CDevImpl &d, YamlState &node)
{
const YAML::PNode  &children( node.lookup(YAML_KEY_children) );
AddChildrenVisitor  visitor( &d, getRegistry() );

#ifdef CPSW_YAML_DEBUG
	fprintf(stderr,"Entering addChildren of %s\n", d.getName());
#endif

	if ( children ) {
#ifdef CPSW_YAML_DEBUG
		fprintf(stderr,"Adding immediate children to %s\n", d.getName());
#endif
		// handle the 'children' node itself
		visitor.visit( &children );
	}

#ifdef CPSW_YAML_DEBUG
	fprintf(stderr,"Leaving addChildren of %s\n", d.getName());
#endif
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
YamlState root( 0, root_name, node[root_name] );

int       vers = -1;

YAML::Node versNode( node[ YAML_KEY_schemaVersionMajor ] );

	if ( versNode ) {
		vers = versNode.as<int>();
	}

	if (   vers < IYamlSupportBase::MIN_SUPPORTED_SCHEMA
	    || vers > IYamlSupportBase::MAX_SUPPORTED_SCHEMA ) {
		char buf[50];
		BadSchemaVersionError e("Yaml Schema: major version ");
		snprintf(buf, sizeof(buf), "%d not supported", vers);
		e.append( std::string(buf) );
		throw e;
	}

	/* Root node must be a Dev */
	return dynamic_pointer_cast<Dev::element_type>( getFieldRegistry_()->makeItem( root ) );
}

static YAML::Node
loadYaml(YamlPreprocessor *preprocessor, StreamMuxBuf *muxer, bool resolveMergeKeys)
{
	try {

		preprocessor->process();

		std::istream top_preprocessed_stream( muxer );

		YAML::Node rootNode( YAML::Load( top_preprocessed_stream ) );

		if ( resolveMergeKeys ) {
			cpsw::resolveMergeKeys( rootNode );
		}

		int vers;
		if ( (vers = preprocessor->getSchemaVersionMajor()) >= 0 ) {
			rootNode[ YAML_KEY_schemaVersionMajor ] = vers;
		}

		if ( (vers = preprocessor->getSchemaVersionMinor()) >= 0 ) {
			rootNode[ YAML_KEY_schemaVersionMinor ] = vers;
		}

		if ( (vers = preprocessor->getSchemaVersionRevision()) >= 0 ) {
			rootNode[ YAML_KEY_schemaVersionRevision ] = vers;
		}


		return rootNode;

	} catch (YAML::Exception &err) {
		// line number is 1-based; but our calculations are zero-based.
		unsigned line = err.mark.line - muxer->getOtherScopeLines() + 1;
		fprintf(stderr,"ERROR: %s\n", err.what());
		fprintf(stderr,"Current file: %s: %u\n", muxer->getFileName(), line);
		throw;
	}
}

static YAML::Node
loadPreprocessedYamlStream(StreamMuxBuf::Stream top_stream, const char *yaml_dir, bool resolveMergeKeys)
{
StreamMuxBuf         muxer;
YamlPreprocessor     preprocessor( top_stream, &muxer, yaml_dir );

	return loadYaml( &preprocessor, &muxer, resolveMergeKeys );
}

class NoOpDeletor {
public:
	void operator()(StreamMuxBuf::Stream::element_type *obj)
	{
	}
};

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(std::istream &top, const char *yaml_dir, bool resolveMergeKeys)
{
StreamMuxBuf         muxer;
StreamMuxBuf::Stream top_stream( &top, NoOpDeletor() );
	return loadPreprocessedYamlStream( top_stream, yaml_dir, resolveMergeKeys );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(const char *yaml, const char *yaml_dir, bool resolveMergeKeys)
{
std::string       str( yaml );
std::stringstream sstrm( str );

	return loadPreprocessedYaml( sstrm, yaml_dir, resolveMergeKeys );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYamlFile(const char *file_name, const char *yaml_dir, bool resolveMergeKeys)
{
StreamMuxBuf         muxer;
YamlPreprocessor     preprocessor( file_name, &muxer, yaml_dir );

	return loadYaml( &preprocessor, &muxer, resolveMergeKeys );
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
		os        << "#schemaversion " << IYamlSupportBase::MAX_SUPPORTED_SCHEMA << ".0.0\n";
		os        << emit.c_str() << "\n";
	} else {
		std::cout << "#schemaversion " << IYamlSupportBase::MAX_SUPPORTED_SCHEMA << ".0.0\n";
		std::cout << emit.c_str() << "\n";
	}
}

YAML::Emitter& operator << (YAML::Emitter& out, const ScalVal_RO& s)
{
	uint64_t u64;
	s->getVal( &u64 );
	out << YAML::BeginMap;
	out << YAML::Key << s->getName();
	out << YAML::Value << u64;
	out << YAML::EndMap;
	return out;
}

YAML::Emitter& operator << (YAML::Emitter& out, const Hub& h)
{
	Children ch = h->getChildren();
	for( unsigned i = 0; i < ch->size(); i++ )
	{
		//       out << ch[i];
	}
	return out;
}

CYamlFieldFactoryBase::FieldRegistry
CYamlFieldFactoryBase::getFieldRegistry_()
{
static FieldRegistry the_registry_( new CYamlTypeRegistry<Field> );
	return the_registry_;
}

DECLARE_YAML_FIELD_FACTORY(EntryImpl);
DECLARE_YAML_FIELD_FACTORY(DevImpl);
DECLARE_YAML_FIELD_FACTORY(IntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(ConstIntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(MemDevImpl);
DECLARE_YAML_FIELD_FACTORY(NullDevImpl);
DECLARE_YAML_FIELD_FACTORY(MMIODevImpl);
DECLARE_YAML_FIELD_FACTORY(NetIODevImpl);
DECLARE_YAML_FIELD_FACTORY(SequenceCommandImpl);

Path
IPath::loadYamlFile(const char *file_name, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name, yaml_dir ) );

	if ( fixup ) {
		YAML::Node root( root_name ? YAML::NodeFind( top, root_name ) : top );
		(*fixup)( root, top );
	}

	Hub rootHub( CYamlFieldFactoryBase::dispatchMakeField( top, root_name ) );
	return IPath::create( rootHub );
}

Path
IPath::loadYamlStream(std::istream &in, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYaml( in, yaml_dir ) );
	if ( fixup ) {
		YAML::Node root( root_name ? YAML::NodeFind( top, root_name ) : top );
		(*fixup)( root, top );
	}

	Hub rootHub( CYamlFieldFactoryBase::dispatchMakeField( top, root_name ) );
	return IPath::create( rootHub );
}

Path
IPath::loadYamlStream(const char *yaml, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name, yaml_dir, fixup );
}

static const YAML::Node findAcrossMerges(const YAML::Node &src, std::string *tokens)
{
	if ( tokens->empty() )
		return src;

const YAML::Node nn = YAML::NodeFind( src, *tokens );

	if ( !!nn ) {
		const YAML::Node nnn = findAcrossMerges( nn, tokens + 1 );
		if ( !! nnn )
			return nnn;
		// else (not found) -> continue at current level and look into
		// merge tag
	}

	// not found; look into merge tag

	const YAML::Node mrg = YAML::NodeFind( src, "<<" );
	if ( mrg )
		return findAcrossMerges(mrg, tokens );
	return mrg;
}

YAML::Node
IYamlFixup::findByName(const YAML::Node &src, const char *path, char sep)
{
const std::string         text( path );
std::vector <std::string> tokens;
std::size_t               st = 0,en;

	while ( (en = text.find(sep, st)) != std::string::npos ) {
		if ( en != st )
			tokens.push_back( text.substr(st, en - st) );
		st = en + 1;
	}
	if ( en != st )
		tokens.push_back( text.substr( st ) );

	tokens.push_back( std::string() );

	return findAcrossMerges( src, & tokens[0] );
}

const YAML::Node
YAML::NodeFind(const YAML::Node &n, const YAML::Node &k)
{
	return static_cast<const YAML::Node>(n)[k];
}


