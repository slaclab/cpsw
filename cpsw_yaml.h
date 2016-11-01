 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_YAML_H
#define CPSW_YAML_H

#include <cpsw_api_builder.h>
#include <cpsw_shared_obj.h>
#include <cpsw_yaml_keydefs.h>

#include <stdio.h>

#include <yaml-cpp/yaml.h>

namespace YAML {
	// A 'Node' extension which remembers parent nodes
	// (which is necessary for backtracking through "<<" merge keys).

	// The idea is that PNodes 'live' as automatic variables on the
	// stack.
	// The 'uplink_' points to the parent node which is on the stack
	// of the current function or one of its callers.
	// The assumption is that this class is used from code which
	// recurses and backtracks through the tree...

	// PNodes observe 'copy-move' semantics. 'parent' and 'child'
	// information of a PNode is erased when it is copied.

	class PNode : public Node {
	private:
		const PNode mutable * parent_;
		const PNode mutable * child_;
		const char          * key_;
		int                   maj_;
		int                   min_;
		int                   rev_;

	public:
		void dump() const;

		const char *getName() const
		{
			return key_;
		}

		const PNode *getParent() const
		{
			return parent_;
		}

		// assign a new Node to this PNode while preserving parent/child/key
		// ('merge' operation)
		PNode & operator=(const Node &orig_node);

		void merge(const Node &orig_node);

		// 'extract' a Node from a PNode
		Node get() const;

		// 'assignment'; moves parent/child from the original to the destination
		PNode & operator=(const PNode &orig);

		// 'copy constructor'; moves parent/child from the original to the destination
		PNode(const PNode &orig);

		// Construct a PNode by map lookup while remembering
		// the parent.
		PNode( const PNode *parent, const char *key );

		// A PNode from a Node
		PNode( const PNode *parent, const char *key, const Node &node );

		// Construct a PNode by sequence lookup while remembering
		// the parent.
		// Note that merge tag backtracking does not support
		// backing up through sequences!
		PNode( const PNode *parent, unsigned index );

		// Root node constructor
		PNode( const char *key, const Node &node );

        // Build a string from all the ancestors to this PNode.
		// Generations are separated by '/'.
		std::string toString() const;

		int getSchemaVersionMajor() const
		{
			return maj_;
		}

		int getSchemaVersionMinor() const
		{
			return min_;
		}

		int getSchemaVersionRevision() const
		{
			return rev_;
		}

		~PNode();

		class MergekeyVisitor {
		public:
			virtual bool visit(PNode *merged_node) = 0;
		};

		// starting at 'this' node visit all merge keys upstream
		// until the visitor's 'visit' method returns 'false', the
		// root is reached or 'maxlevel' parents have been scanned
		// for merge keys.
		void visitMergekeys(MergekeyVisitor *, int maxlevel = -1);

		// lookup a key starting at 'this' PNode and backtrack
		// through merge ("<<") keys.
		//
		// If there is a structure like this
		//
		//   a:
		//     <<:
		//       b:
		//         c:
		//           key1: merged_value1
		//           key2: merged_value2
		//     b:
		//       c:
		//         key1: value1:
		//
		// then the algorithm is expected to
		// find
		//    a["b"]["c"]["key1"] -> value1
		//    a["b"]["c"]["key2"] -> merged_value2
		//
		// Since recursive lookups through many merge keys can become
		// quite expensive we provide the 'maxlevel' feature which
		// tells the algorithm through how many levels of parents it
		// should search for merge keys (0: none, <0 all levels).
		PNode lookup(const char *key, int maxlevel = -1) const;

	private:
		static YAML::Node backtrack_mergekeys(const YAML::PNode *, unsigned, const YAML::Node &, int);
	};
};

typedef const YAML::PNode YamlState;

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
		static void overrideNode(YamlState &orig);

		// insert the classname into a node;
		// e.g.,:
		//   node[YAML_KEY_class] = getClassName();
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
				else if (str.compare( "UNKNOWN" ) == 0 )
					rhs = UNKNOWN;
				else
					return false;

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
				else if (str.compare( "UNKNOWN_CACHEABLE" ) == 0 )
					rhs = IField::UNKNOWN_CACHEABLE;
				else
					return false;

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
		struct convert<IScalVal_Base::Encoding> {
			static bool decode(const Node& node, IScalVal_Base::Encoding& rhs) {


				if (!node.IsScalar())
					return false;

				std::string str = node.Scalar();
				unsigned          num;

				if      ( str.compare( "NONE"   ) == 0 )
					rhs = IScalVal_Base::NONE;
				else if ( str.compare( "ASCII"  ) == 0 )
					rhs = IScalVal_Base::ASCII;
				else if ( str.compare( "EBCDIC" ) == 0 )
					rhs = IScalVal_Base::EBCDIC;
				else if ( str.compare( "UTF_8"  ) == 0 )
					rhs = IScalVal_Base::UTF_8;
				else if ( str.compare( "UTF_16" ) == 0 )
					rhs = IScalVal_Base::UTF_16;
				else if ( str.compare( "UTF_32" ) == 0 )
					rhs = IScalVal_Base::UTF_32;
				else if ( 1 == ::sscanf( str.c_str(), "ISO_8859_%d", &num ) && num >=1 && num <=16 )
					rhs = static_cast<IScalVal_Base::Encoding>( IScalVal_Base::ISO_8859_1 + num - 1 );
				else if ( 1 == ::sscanf( str.c_str(), "CUSTOM_%i", &num ) )
					rhs = static_cast<IScalVal_Base::Encoding>( IScalVal_Base::CUSTOM + num );
				else
					return false;

				return true;
			}

			static inline std::string
				do_encode(const IScalVal_Base::Encoding &rhs)
				{
					if      ( IScalVal_Base::NONE   == rhs )
						return std::string("NONE");
					else if ( IScalVal_Base::ASCII  == rhs )
						return std::string("ASCII");
					else if ( IScalVal_Base::EBCDIC == rhs )
						return std::string("EBCDIC");
					else if ( IScalVal_Base::UTF_8  == rhs )
						return std::string("UTF_8");
					else if ( IScalVal_Base::UTF_16 == rhs )
						return std::string("UTF_16");
					else if ( IScalVal_Base::UTF_32 == rhs )
						return std::string("UTF_32");
					else if ( IScalVal_Base::ISO_8859_1 <= rhs && IScalVal_Base::ISO_8859_16 >= rhs ) {
						char buf[100];
						snprintf(buf, sizeof(buf), "ISO_8859_%d", (unsigned)rhs - IScalVal_Base::ISO_8859_1 + 1);
						return std::string(buf);
					} else if ( IScalVal_Base::CUSTOM <= rhs ) {
						char buf[100];
						snprintf(buf, sizeof(buf), "CUSTOM_%d", (unsigned)rhs - IScalVal_Base::CUSTOM);
						return std::string(buf);
					} else {
						return std::string("NONE");
					}
				}

			static Node encode(const IScalVal_Base::Encoding& rhs) {

				Node node( do_encode( rhs ) );

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

}

// helpers to read map entries

template <typename T> static void mustReadNode(YamlState &node, const char *fld, T *val)
{
	const YAML::Node &n( node.lookup(fld) );
	if ( ! n ) {
		throw NotFoundError( std::string("property '") + std::string(fld) + std::string("'") );
	} else {
        if ( n.IsNull() ) {
		   throw InvalidArgError( std::string("property '") + std::string(fld) + std::string("' is NULL") );
        }
		*val = n.as<T>();
	}
}

template <typename T> static bool readNode(YamlState &node, const char *fld, T *val)
{
	const YAML::Node &n( node.lookup(fld) );
	if ( n && ! n.IsNull() ) {
		*val = n.as<T>();
		return true;
	}
	return false;
}

template <typename T> static void writeNode(YAML::Node &node, const char *fld, const T &val)
{
	node[fld] = val;
}

void pushNode(YAML::Node &node, const char *fld, const YAML::Node &child);

// YAML Emitter overloads

YAML::Emitter& operator << (YAML::Emitter& out, const ScalVal_RO& s);
YAML::Emitter& operator << (YAML::Emitter& out, const Hub& h);


template <typename T> class IYamlFactoryBase;

template <typename T> class IYamlTypeRegistry {
public:

	virtual T    makeItem(YamlState &) = 0;

	virtual void extractClassName(std::vector<std::string> *svec_p, YamlState &)  = 0;

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

    // If 'dynLoad' is 'true' then 'getItem("xxx")' (on failure)
	// shall try to load "xxx.so" and try again.
	static IRegistry *create(bool dynLoad);
};

template <typename T> class IYamlFactoryBase {
public:
	typedef shared_ptr< IYamlTypeRegistry<T> > Registry;
private:
	Registry     registry_	;
	const char  *className_;
public:
	IYamlFactoryBase(const char *className, Registry r)
	: registry_(r),
	  className_(className)
	{
		r->addFactory( className, this );
	}

	virtual T makeItem(YamlState &node ) = 0;

	virtual Registry getRegistry()
	{
		return registry_;
	}

	virtual ~IYamlFactoryBase()
	{
		printf("Destroying factory %s\n", className_);
		registry_->delFactory( className_ );
	}
};

template <typename T> class CYamlTypeRegistry : public IYamlTypeRegistry<T> {
private:

	IRegistry *registry_;

	CYamlTypeRegistry(const CYamlTypeRegistry&);
	CYamlTypeRegistry operator=(const CYamlTypeRegistry &);

public:
	CYamlTypeRegistry(bool dynLoadSupport = true)
	: registry_( IRegistry::create(dynLoadSupport) )
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

	virtual void extractClassName(std::vector<std::string> *svec_p, YamlState &n);

	virtual bool extractInstantiate(YamlState &n);

	virtual T makeItem(YamlState &n)
	{
	std::vector<std::string> str_vec;
	std::string str_no_factory = "";

		if ( ! extractInstantiate( n ) )
			return T();

		extractClassName( &str_vec, n  );
		if ( str_vec.size() == 0 )
			throw NotFoundError("CYamlTypeRegistry: node w/o class info");

		for( std::vector<std::string>::iterator it = str_vec.begin(); it != str_vec.end(); ++it ) {
			IYamlFactoryBase<T> *factory = static_cast<IYamlFactoryBase<T> *>( registry_->getItem( it->c_str() ) );
			if ( factory )
				return factory->makeItem( n );
			if ( 0 != str_no_factory.size() )
				str_no_factory += ", ";
			str_no_factory += (*it);
		}

		throw NotFoundError(std::string("No factory for '") + str_no_factory + std::string("' found"));
	}

	virtual void dumpClasses()
	{
		registry_->dumpItems();
	}

	virtual ~CYamlTypeRegistry()
	{
		printf("Destroying a type registry\n");
		delete registry_;
	}
};

class CYamlFieldFactoryBase : public IYamlFactoryBase<Field> {
	public:
		typedef IYamlFactoryBase<Field>::Registry FieldRegistry;

		static FieldRegistry getFieldRegistry_();
	protected:
		CYamlFieldFactoryBase(const char *typeLabel)
		: IYamlFactoryBase<Field>( typeLabel, getFieldRegistry_() )
		{
		}

		virtual void addChildren(CEntryImpl &, YamlState &);
		virtual void addChildren(CDevImpl &,   YamlState &);


	public:
		static Dev dispatchMakeField(const YAML::Node &node, const char *root_name);

		static YAML::Node loadPreprocessedYaml    (std::istream &,          const char *yaml_dir = 0);
		static YAML::Node loadPreprocessedYaml    (const char *char_stream, const char *yaml_dir = 0);
		static YAML::Node loadPreprocessedYamlFile(const char *file_name,   const char *yaml_dir = 0);

		static void dumpClasses() { getFieldRegistry_()->dumpClasses(); }
};


template <typename T> class CYamlFieldFactory : public CYamlFieldFactoryBase {
public:
	CYamlFieldFactory()
	: CYamlFieldFactoryBase(T::element_type::_getClassName())
	{
	}

	virtual Field makeItem(YamlState & state)
	{
	    T::element_type::overrideNode(state);
		T fld( CShObj::create<T, YamlState &>(state) );
		addChildren( *fld, state );
		return fld;
	}

};

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
