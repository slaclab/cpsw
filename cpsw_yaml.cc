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

#include <boost/unordered_set.hpp>

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
#include <cpsw_mmio_dev.h>
#include <cpsw_netio_dev.h>
#include <cpsw_command.h>
#include <cpsw_preproc.h>

#include <dlfcn.h>

using boost::dynamic_pointer_cast;
using boost::unordered_set;

using YAML::PNode;
using YAML::Node;
using std::cout;

#undef CPSW_YAML_DEBUG

// Helper operations
struct CStrOps {
	struct Hash {
		size_t operator() (const char *str) const
		{
		size_t hash = 5381;
		size_t c;
			while ( (c = *str++) ) {
				hash = (hash << 5) + hash + c;
			}
			return hash;
		}
	};

	struct Eq {
		bool operator() (const char *a, const char *b) const
		{
			return 0 == ::strcmp(a, b);
		}
	};

	struct Cmp {
		bool operator () (const char *a, const char *b) const {
			return ::strcmp(a,b) < 0 ? true : false;
		}
	};
};

template <typename T>
struct CStrMap : public CStrOps {
	typedef std::pair<const char *, T>      Member;
	typedef std::map <const char *, T, Cmp> Map;
};

struct CStrSet : public CStrOps {
	typedef unordered_set<const char *, Hash, Eq> Set;
};

// PNode Implementation

void
PNode::dump() const
{
	printf("PNode key: %s, p: %p, c: %p\n" , key_ , parent_, child_ );
}


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

void
PNode::merge(const Node &orig_node)
{
	*this = orig_node;
}

// 'extract' a Node from a PNode
Node
PNode::get() const
{
	return * static_cast<const YAML::Node *>(this);
}

// 'assignment'; moves parent/child from the original to the destination
PNode &
PNode::operator=(const PNode &orig)
{
	if ( this != &orig ) {
		if ( orig.child_ )
			throw InternalError("can only copy a PNode at the tail of a chain");
		static_cast<YAML::Node &>(*this) = static_cast<const YAML::Node &>(orig);
		parent_ = orig.parent_;
		child_  = orig.child_;
		key_    = orig.key_;
		if ( parent_ )
			parent_->child_ = this;
		orig.parent_ = 0;
		orig.child_  = 0;
		maj_ = orig.maj_;
		min_ = orig.min_;
		rev_ = orig.rev_;
	}
	return *this;
}

// 'copy constructor'; moves parent/child from the original to the destination
PNode::PNode(const PNode &orig)
: Node( static_cast<const YAML::Node&>(orig) ),
  maj_( orig.maj_ ),
  min_( orig.min_ ),
  rev_( orig.rev_ )
{
	parent_ = orig.parent_;
	child_  = orig.child_;
	key_    = orig.key_;
	if ( parent_ )
		parent_->child_ = this;
	orig.parent_ = 0;
	orig.child_  = 0;
}

PNode::PNode( const PNode *parent, const char *key, const Node &node)
: Node( node ),
  parent_( parent ),
  child_ ( 0 ),
  key_( key ),
  maj_( -1 ),
  min_( -1 ),
  rev_( -1 )
{
	if ( parent_ ) {
		parent_->child_ = this;
		maj_ = parent->maj_;
		min_ = parent->min_;
		rev_ = parent->rev_;
	}
}

PNode::PNode( const char *key, const Node &top )
: Node( top[key] ),
  parent_( 0 ),
  child_( 0 ),
  key_( key ),
  maj_( -1 ),
  min_( -1 ),
  rev_( -1 )
{
	{
	YAML::Node n(top[ YAML_KEY_schemaVersionMajor    ]);
	if ( n )
		maj_ = n.as<int>();
	}
	{
	YAML::Node n(top[ YAML_KEY_schemaVersionMinor    ]);
	if ( n )
		min_ = n.as<int>();
	}
	{
	YAML::Node n(top[ YAML_KEY_schemaVersionRevision ]);
	if ( n )
		rev_ = n.as<int>();
	}
}

std::string
PNode::toString() const
{
	if ( parent_ )
		return parent_->toString() + std::string("/") + std::string(key_);
	return std::string("key_");
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
: Node( fixInvalidNode((*parent)[key]) ),
  parent_(parent),
  child_(0),
  key_(key)
{
	if ( parent->child_ )
		throw   InternalError(std::string("Parent ")
			  + parent->toString()
			  + std::string(" asready has a child!")
			    );
	parent_->child_ = this;
	maj_            = parent->maj_;
	min_            = parent->min_;
	rev_            = parent->rev_;
}

// Construct a PNode by sequence lookup while remembering
// the parent.
PNode::PNode( const PNode *parent, unsigned index)
: Node( fixInvalidNode( (*parent)[index] ) ),
  parent_(parent),
  child_(0),
  key_(0)
{
	parent_->child_ = this;
	maj_            = parent->maj_;
	min_            = parent->min_;
	rev_            = parent->rev_;
}

PNode::~PNode()
{
	if ( parent_ )
		parent_->child_ = 0;
}


Node
PNode::backtrack_mergekeys(const PNode *path_head, unsigned path_nelms, const Node &top, int lvl)
{
int  i;
Node nodes[path_nelms+1];

	if ( ! top.IsMap() )
		throw   ConfigurationError(std::string("Value of merge key (in: ")
			  + path_head->toString()
			  + std::string(") is not a Map")
			    );

	// see comments in PNode::operator=(const Node &)
	nodes[0].reset( top );

	i=0;

#ifdef CPSW_YAML_DEBUG
	printf("%*sbacktrack_mergekeys: ENTERING (head %s)\n", lvl,"", path_head->key_);
#endif

	// search right
	while ( path_head && (nodes[i+1].reset( fixInvalidNode( nodes[i][path_head->key_] ) ), nodes[i+1]) ) {
#ifdef CPSW_YAML_DEBUG
		printf("%*sbacktrack_mergekeys: found right: %s\n", lvl, "", path_head->key_);
#endif
		path_head = path_head->child_;
		i++;
	}

	if ( ! path_head ) {
		/* found the last element */
#ifdef CPSW_YAML_DEBUG
		printf("%*sbacktrack_mergekeys: RETURN -- FOUND\n", lvl, "");
#endif
		return nodes[i];
	}

	/* not found in right subtree; backtrack */
	while ( i >= 0 ) {

		// see comments in PNode::operator=(const Node &)
		nodes[i].reset( fixInvalidNode( nodes[i][YAML_KEY_MERGE] ) );
		if ( nodes[i] ) {
#ifdef CPSW_YAML_DEBUG
            printf("%*sbacktrack_mergekeys: found MERGE (level %i -- key %s)\n", lvl, "", i, path_head->key_);
#endif
			const Node n( backtrack_mergekeys(path_head, path_nelms - i, nodes[i], lvl+1) );
			if ( n ) {
#ifdef CPSW_YAML_DEBUG
				printf("%*sbacktrack_mergekeys: MERGE SUCCESS\n", lvl, "");
#endif
				return n;
			}
		} else {
#ifdef CPSW_YAML_DEBUG
            printf("%*sbacktrack_mergekeys: no MERGE (level %i -- key %s)\n", lvl, "", i, path_head->key_);
#endif
		}

		path_head = path_head->parent_;
		--i;
	}
#ifdef CPSW_YAML_DEBUG
	printf("%*sbacktrack_mergekeys: RETURN Undef\n", lvl, "");
#endif

	return Node ( YAML::NodeType::Undefined );
}

void
isd(PNode *pn)
{
if ( pn->IsDefined() )
	cout << *pn << "\n";
else
	cout << "PN UNDEFINED?\n";
}


#ifdef CPSW_YAML_DEBUG
static void pp(const PNode *p)
{
	if ( p ) {
		pp(p->getParent());
		printf("/%s", p->getName());
	}
}
#endif

// starting at 'this' node visit all merge keys upstream
// until the visitor's 'visit' method returns 'false'.
void
PNode::visitMergekeys(MergekeyVisitor *visitor, int maxlevel)
{
const PNode *top   = this->parent_;
unsigned     nelms = 1;

#ifdef CPSW_YAML_DEBUG
	printf("visitMergekeys: ENTERING: "); pp(top); printf("/%s\n", getName());
#endif

	while ( top && maxlevel ) {

#ifdef CPSW_YAML_DEBUG
        printf("visitMergekeys: %s in %s\n", this->key_, top->key_);
#endif

		if ( ! top->IsMap() ) {
			return; // cannot handle merge keys across sequences
		}

		// no need for 'fixInvalidNode()' since 'merged_node' is only
		// operated on if it is not 'undefined'.
		const YAML::Node &merged_node( (*top)[YAML_KEY_MERGE] );

		if ( merged_node ) {
			// try (backtracking) lookup from the node at the mergekey
			merge( backtrack_mergekeys(top->child_, nelms , merged_node, 0) );
			if ( *this && ! visitor->visit( this ) )
				return;
		}

		// were not lucky in 'node'; continue searching one level up
		top = top->parent_;

		// each time we back up we must search for a longer path in the
		// merged node...
		nelms++;
#ifdef CPSW_YAML_DEBUG
        printf("visitMergekeys: trying one level up\n");
#endif

		if ( maxlevel > 0 )
			maxlevel--;
	}

}

class LookupVisitor : public PNode::MergekeyVisitor {
public:
	virtual bool visit(PNode *merged_node)
	{
		return false;
	}
};


PNode
PNode::lookup(const char *key, int maxlevel) const
{
// a frequent case involves no merge key; try a direct lookup first.
PNode         node(this, key);
LookupVisitor visitor;
	if ( ! node ) {
		node.visitMergekeys( &visitor, maxlevel );
	}
/* FIXME?? Don't remember why I thought a Null node (which
   passes ! operator!(), i.e., is 'true') should be resolved
   to an undefined one.
   For what I know Null nodes should be 'legal'.
	if ( node.IsNull() ) {
		node.merge( Node( YAML::NodeType::Undefined ) );
	}
*/
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

	typedef CStrMap<void*>::Map    Map;
	typedef CStrMap<void*>::Member Member;
    typedef CStrSet::Set           Set;

private:
	Map  map_;
    Set  failed_;
	bool dynLoad_;

public:
    RegistryImpl(bool dynLoad)
	: dynLoad_(dynLoad)
	{
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
			  + std::string("' in: ") + node.getName()
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
CYamlFieldFactoryBase::addChildren(CEntryImpl &e, YamlState &node, IYamlTypeRegistry<Field> *registry)
{
	// nothing to do for 'leaf' entries
}

// The AddChildrenVisitor handles 'upstream' merge keys:
//
//   main:
//     <<:
//       from_upstream:
//     children:
//       here:
//
// so that both 'here' and 'from_upstream' are instantiated.
// This works through multiple levels up to the top, so that
//
// <<:
//   main:
//     children:
//       from_up_up:
//
// would also instantiate 'from_up_up'.
//
//
// Code inside the 'visitor' method makes sure merge keys at
// the same level than children are followed:
//
//     children:
//       child:
//       <<:
//         sibling1:
//         <<:
//           sibling2:
//
// i.e., in addition to 'child', 'sibling1' and 'sibling2' are
// created.
class AddChildrenVisitor : public PNode::MergekeyVisitor {
private:
	CDevImpl                *d_;
	IYamlTypeRegistry<Field> *registry_;


	// record children that were not created because 'instantiated' is false
	// we need to remember this when going through merge keys upstream
	// (which may contain the same child marked as instantiated = true).
	unordered_set<std::string> not_instantiated_;

public:
	AddChildrenVisitor(CDevImpl *d, IYamlTypeRegistry<Field> *registry)
	: d_(d),
	  registry_(registry)
	{
	}

	virtual bool visit(PNode *merged_node)
	{

		while ( *merged_node ) {

			YAML::const_iterator it( merged_node->begin() );
			YAML::const_iterator ite( merged_node->end()  );

			YAML::Node downmerged_node( YAML::NodeType::Undefined );
			while ( it != ite ) {
				const std::string &k = it->first.as<std::string>();

				// skip merge node! But remember it and follow downstream
				// after all other children are handled.
				if ( 0 == k.compare( YAML_KEY_MERGE ) ) {
#ifdef CPSW_YAML_DEBUG
					printf("AddChildrenVisitor: found a merge node\n");
#endif
					if ( it->second && ! it->second.IsMap() ) {
						throw ConfigurationError(
							  std::string("Value of merge key (in: ")
							+ merged_node->toString()
							+ std::string(") is not a Map")
							  );
					}
					// see comments in PNode::operator=(const Node &)
					downmerged_node.reset( it->second );
				} else {
					// It is possible that the child already exists because
					// we are now visiting a merged node.
					// The child was created as a result from visiting
					// a 'non-merged' node (or a merged node with higher precedence).
					// We simply skip creation in this case.
					//
					// E.g.,
					//    children:
					//       <<:
					//         child:
					//            key: defaultval
					//       child:
					//         class: theclass
					//
					// would first visit 'child' and create it (class:theclass, key: defaultval)
					// then go into the merged node, find 'child' thereunder and skip a second
					// instantiation.
					if ( ! d_->getChild( k.c_str() ) && 0 == not_instantiated_.count( k ) ) {
						const YAML::PNode child( merged_node, k.c_str(), it->second );
#ifdef CPSW_YAML_DEBUG
						printf("AddChildrenVisitor: trying to make child %s\n", k.c_str());
#endif
						Field c = registry_->makeItem( child );
						if ( c ) {
							// if 'instantiate' is 'false' then
							// makeItem() returns a NULL pointer
#ifdef CPSW_YAML_DEBUG
							printf("AddChildrenVisitor: adding child %s\n", k.c_str());
#endif

							const YAML::PNode child_address( child.lookup(YAML_KEY_at) );
							if ( child_address ) {
								d_->addAtAddress( c, child_address );
							} else {
								not_instantiated_.insert( k );
								std::string errmsg =   std::string("Child '")
								                     + child.toString()
								                     + std::string("' found but missing '"YAML_KEY_at"' key");
								throw InvalidArgError(errmsg);
							}
						} else {
#ifdef CPSW_YAML_DEBUG
							printf("AddChildrenVisitor: adding child %s to 'not_instantiated' database\n", k.c_str());
#endif
							not_instantiated_.insert( k );
						}
					} else {
#ifdef CPSW_YAML_DEBUG
						printf("AddChildrenVisitor: %s not instantiated\n", k.c_str());
#endif
					}
				}
				++it;
			}
			merged_node->merge( downmerged_node );
		}
		return true; // continue looking for merge keys upstream
	}
};

void
CYamlFieldFactoryBase::addChildren(CDevImpl &d, YamlState &node, IYamlTypeRegistry<Field> *registry)
{
YAML::PNode         children( node.lookup(YAML_KEY_children) );
AddChildrenVisitor  visitor( &d, registry );

#ifdef CPSW_YAML_DEBUG
	printf("Entering addChildren of %s\n", d.getName());
#endif

	if ( children ) {
#ifdef CPSW_YAML_DEBUG
		printf("Adding immediate children to %s\n", d.getName());
#endif
		// handle the 'children' node itself
		visitor.visit( &children );
	}

#ifdef CPSW_YAML_DEBUG
	printf("Looking for merged children of %s\n", d.getName());
#endif
	// and now look for merge keys upstream which may match
	// our node
	children.visitMergekeys( &visitor );
#ifdef CPSW_YAML_DEBUG
	printf("Leaving addChildren of %s\n", d.getName());
#endif
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
YamlState root( root_name, node );
int       vers = root.getSchemaVersionMajor();

	if (   vers < IYamlSupportBase::MIN_SUPPORTED_SCHEMA
	    || vers > IYamlSupportBase::MAX_SUPPORTED_SCHEMA ) {
		char buf[50];
		BadSchemaVersionError e("Yaml Schema: major version ");
		snprintf(buf, sizeof(buf), "%d not supported", vers);
		e.append( std::string(buf) );
		throw e;
	}

	/* Root node must be a Dev */
	return dynamic_pointer_cast<Dev::element_type>( getFieldRegistry()->makeItem( root ) );
}

static YAML::Node
loadPreprocessedYamlStream(StreamMuxBuf::Stream top_stream, const char *yaml_dir)
{
StreamMuxBuf         muxer;

YamlPreprocessor     preprocessor( top_stream, &muxer, yaml_dir );

	preprocessor.process();

	std::istream top_preprocessed_stream( &muxer );

	YAML::Node rootNode( YAML::Load( top_preprocessed_stream ) );

	int vers;
	if ( (vers = preprocessor.getSchemaVersionMajor()) >= 0 ) {
		rootNode[ YAML_KEY_schemaVersionMajor ] = vers;
	}

	if ( (vers = preprocessor.getSchemaVersionMinor()) >= 0 ) {
		rootNode[ YAML_KEY_schemaVersionMinor ] = vers;
	}

	if ( (vers = preprocessor.getSchemaVersionRevision()) >= 0 ) {
		rootNode[ YAML_KEY_schemaVersionRevision ] = vers;
	}
			

	return rootNode;
}

class NoOpDeletor {
public:
	void operator()(StreamMuxBuf::Stream::element_type *obj)
	{
	}
};

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(std::istream &top, const char *yaml_dir)
{
StreamMuxBuf         muxer;
StreamMuxBuf::Stream top_stream( &top, NoOpDeletor() );
	return loadPreprocessedYamlStream( top_stream, yaml_dir );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYaml(const char *yaml, const char *yaml_dir)
{
std::string       str( yaml );
std::stringstream sstrm( str );

	return loadPreprocessedYaml( sstrm, yaml_dir );
}

YAML::Node
CYamlFieldFactoryBase::loadPreprocessedYamlFile(const char *file_name, const char *yaml_dir)
{
const char  *sep;
std::string  main_dir;
	if ( ! yaml_dir && (sep = ::strrchr(file_name,'/')) ) {
		main_dir = std::string(file_name);
		main_dir.resize(sep - file_name);
		yaml_dir = main_dir.c_str();
	}
	return loadPreprocessedYamlStream( StreamMuxBuf::mkstrm( file_name ), yaml_dir );
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

IYamlTypeRegistry<Field> *
CYamlFieldFactoryBase::getFieldRegistry()
{
static CYamlTypeRegistry<Field> the_registry_;
	return &the_registry_;
}

DECLARE_YAML_FIELD_FACTORY(EntryImpl);
DECLARE_YAML_FIELD_FACTORY(DevImpl);
DECLARE_YAML_FIELD_FACTORY(IntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(ConstIntEntryImpl);
DECLARE_YAML_FIELD_FACTORY(MemDevImpl);
DECLARE_YAML_FIELD_FACTORY(MMIODevImpl);
DECLARE_YAML_FIELD_FACTORY(NetIODevImpl);
DECLARE_YAML_FIELD_FACTORY(SequenceCommandImpl);

Hub
IHub::loadYamlFile(const char *file_name, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name, yaml_dir ) );

	if ( fixup ) {
		YAML::Node root( root_name ? top[root_name] : top );
		(*fixup)( root );
	}

	return CYamlFieldFactoryBase::dispatchMakeField( top, root_name );
}

Hub
IHub::loadYamlStream(std::istream &in, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYaml( in, yaml_dir ) );
	if ( fixup ) {
		YAML::Node root( root_name ? top[root_name] : top );
		(*fixup)( root );
	}

	return CYamlFieldFactoryBase::dispatchMakeField( top, root_name );
}

Hub
IHub::loadYamlStream(const char *yaml, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name, yaml_dir, fixup );
}

Path
IPath::loadYamlFile(const char *file_name, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name, yaml_dir ) );

	if ( fixup ) {
		YAML::Node root( root_name ? top[root_name] : top );
		(*fixup)( root );
	}

	Hub rootHub( CYamlFieldFactoryBase::dispatchMakeField( top, root_name ) );
	return IPath::create( rootHub );
}

Path
IPath::loadYamlStream(std::istream &in, const char *root_name, const char *yaml_dir, IYamlFixup *fixup)
{
	YAML::Node top( CYamlFieldFactoryBase::loadPreprocessedYaml( in, yaml_dir ) );
	if ( fixup ) {
		YAML::Node root( root_name ? top[root_name] : top );
		(*fixup)( root );
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
