#include <sstream>
#include <fstream>
#include <iostream>

#include <boost/unordered_set.hpp>

#include <string.h>

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
using boost::unordered_set;

using YAML::PNode;
using YAML::Node;
using std::cout;

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
			throw "can only copy a PNode at the tail of a chain";
		static_cast<YAML::Node &>(*this) = static_cast<const YAML::Node &>(orig);
		parent_ = orig.parent_;
		child_  = orig.child_;
		key_    = orig.key_;
		if ( parent_ )
			parent_->child_ = this;
		orig.parent_ = 0;
		orig.child_  = 0;
	}
	return *this;
}

// 'copy constructor'; moves parent/child from the original to the destination
PNode::PNode(const PNode &orig)
: Node( static_cast<const YAML::Node&>(orig) )
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
  key_( key )
{
	if ( parent_ )
		parent_->child_ = this;
}


// Construct a PNode by map lookup while remembering
// the parent.
PNode::PNode( const PNode *parent, const char *key )
: Node( YAML::NodeType::Undefined ),
  parent_(parent),
  child_(0),
  key_(key)
{
	if ( parent->child_ )
		throw "Parent has a child!";
	parent_->child_ = this;

	if ( (*parent)[key] )
		*this = (*parent)[key];
		
}

// Construct a PNode by sequence lookup while remembering
// the parent.
PNode::PNode( const PNode *parent, unsigned index)
: Node( (*parent)[index] ),
  parent_(parent),
  child_(0),
  key_(0)
{
	if ( parent_ )
		parent_->child_ = this;
}

PNode::~PNode()
{
	if ( parent_ )
		parent_->child_ = 0;
}


Node 
PNode::backtrack_mergekeys(const PNode *path_head, unsigned path_nelms, const Node &top)
{
int  i;
Node nodes[path_nelms+1];

	// see comments in PNode::operator=(const Node &)
	nodes[0].reset( top );

	i=0;

	// search right
	while ( path_head && (nodes[i+1].reset( nodes[i][path_head->key_] ), nodes[i+1]) ) {
		path_head = path_head->child_;
		i++;
	}

	if ( ! path_head ) {
		/* found the last element */
		return nodes[i];
	}

	/* not found in right subtree; backtrack */
	while ( i >= 0 ) {

		// see comments in PNode::operator=(const Node &)
		nodes[i].reset(nodes[i]["<<"]);
		if ( nodes[i] ) {
			const Node n( backtrack_mergekeys(path_head, path_nelms - i, nodes[i]) );
			if ( n )
				return n;
		}

		path_head = path_head->parent_;
		--i;
	}

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

		// starting at 'this' node visit all merge keys upstream
		// until the visitor's 'visit' method returns 'false'.
void
PNode::visitMergekeys(MergekeyVisitor *visitor, int maxlevel)
{
const PNode *top   = this->parent_;
unsigned     nelms = 1;

	while ( top && maxlevel ) {

		if ( ! top->IsMap() ) {
			std::cerr << "WARNING: Cannot backtrack merge keys across sequences!";
			return; // cannot handle merge keys across sequences
		}

		const YAML::Node &merged_node = (*top)["<<"];

		if ( merged_node ) {
			// try (backtracking) lookup from the node at the mergekey
			merge( backtrack_mergekeys(top->child_, nelms , merged_node) );
			if ( *this && ! visitor->visit( this ) )
				return;
		}

		// were not lucky in 'node'; continue searching one level up
		top = top->parent_;

		// each time we back up we must search for a longer path in the
		// merged node...
		nelms++;

		if ( maxlevel > 0 )
			maxlevel--;
	}

}

class LookupVisitor : public PNode::MergekeyVisitor {
public:
	virtual bool visit(PNode *merged_node)
	{
		// 
		return false;
	}
};


PNode
PNode::lookup(const char *key, int maxlevel) const
{
// a frequent case involves no merge key; try a direct lookup first.
PNode         node(this, key);
LookupVisitor visitor;
	if ( node.IsNull()  || ! node.IsDefined() ) {
		node.visitMergekeys( &visitor, maxlevel );
	}
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

	struct StrHash {
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

	struct StrCmp {
		bool operator() (const char *a, const char *b) const
		{
			return 0 == ::strcmp(a, b);
		}
	};

	// record children that were not created because 'instantiated' is false
	// we need to remember this when going through merge keys upstream
	// (which may contain the same child marked as instantiated = true).
	unordered_set<const char*, StrHash, StrCmp>  not_instantiated_;

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
				const char *k = it->first.as<std::string>().c_str();

				// skip merge node! But remember it and follow downstream
				// after all other children are handled.
				if ( 0 == strcmp( k, "<<" ) ) {
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
					if ( ! d_->getChild( k ) && 0 == not_instantiated_.count( k ) ) {
						const YAML::PNode child( merged_node, k, it->second );
						Field c = registry_->makeItem( child );
						if ( c ) {
							// if 'instantiate' is 'false' then
							// makeItem() returns a NULL pointer
							d_->addAtAddress( c, child );
						} else {
							not_instantiated_.insert( k );
						}
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
YAML::PNode         children( node.lookup("children") );
AddChildrenVisitor  visitor( &d, registry );

	if ( children ) {
		// handle the 'children' node itself
		visitor.visit( &children );
	}

	// and now look for merge keys upstream which may match
	// our node
	children.visitMergekeys( &visitor );
}

Dev
CYamlFieldFactoryBase::dispatchMakeField(const YAML::Node &node, const char *root_name)
{
YamlState root( 0, root_name, root_name ? node[root_name] : node );
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

		return YAML::Load( top_preprocessed_stream );
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
IHub::loadYamlFile(const char *file_name, const char *root_name, const char *yaml_dir)
{
#ifdef NO_YAML_SUPPORT
	throw NoYAMLSupportError();
#else
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYamlFile( file_name, yaml_dir ), root_name );
#endif
}

Hub
IHub::loadYamlStream(std::istream &in, const char *root_name, const char *yaml_dir)
{
#ifdef NO_YAML_SUPPORT
	throw NoYAMLSupportError();
#else
	return CYamlFieldFactoryBase::dispatchMakeField( CYamlFieldFactoryBase::loadPreprocessedYaml( in, yaml_dir ), root_name );
#endif
}

Hub
IHub::loadYamlStream(const char *yaml, const char *root_name, const char *yaml_dir)
{
std::string  str( yaml );
std::stringstream sstrm( str );
	return loadYamlStream( sstrm, root_name, yaml_dir );
}

#ifdef NO_YAML_SUPPORT
void
IYamlSupport::dumpYamlFile(Entry top, const char *file_name, const char *root_name)
{
	throw NoYAMLSupportError();
}
#endif
