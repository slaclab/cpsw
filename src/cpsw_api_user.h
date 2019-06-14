 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_API_USER_H
#define CPSW_API_USER_H

#include <stdint.h>
#include <list>
#include <string.h>
#include <vector>
#include <string>

#include <cpsw_shared_ptr.h>

class IEntry;
class IHub;
class IChild;
class IPath;
class IVal_Base;
class IScalVal_Base;
class IScalVal_RO;
class IScalVal_WO;
class IScalVal;
class IEnum;
class IStream;
class IDoubleVal_RO;
class IDoubleVal_WO;
class IDoubleVal;

typedef shared_ptr<const IEntry>         Entry;
typedef shared_ptr<const IHub>           Hub;
typedef shared_ptr<const IChild>         Child;
typedef shared_ptr<IPath>                Path;
typedef shared_ptr<const IPath>          ConstPath;
typedef shared_ptr<IVal_Base>            Val_Base;
typedef shared_ptr<IScalVal_Base>        ScalVal_Base;
typedef shared_ptr<IScalVal_RO>          ScalVal_RO;
typedef shared_ptr<IScalVal_WO>          ScalVal_WO;
typedef shared_ptr<IScalVal>             ScalVal;
typedef shared_ptr<IEnum>                Enum;
typedef shared_ptr<const std::string>    CString;
typedef shared_ptr<IStream>              Stream;
typedef shared_ptr< std::vector<Child> > Children;
typedef shared_ptr<IDoubleVal_RO>        DoubleVal_RO;
typedef shared_ptr<IDoubleVal_WO>        DoubleVal_WO;
typedef shared_ptr<IDoubleVal>           DoubleVal;

#include <cpsw_error.h>

namespace YAML {
	class Node;

	// YAML-CPP's map-lookup operator 'operator[]'
	// has very cumbersome semantics:
	//
	//   Node operator[](const Key &);
	//
	// actually creates a new map entry (and breaks
	// iterators as of version 0.6.2; even though a
	// new map entry may not be 'visible' if it is
	// not assigned a value the operation voids
	// existing iterators -- the latter did not
	// happen under 0.5.3!).
	//
	// However,
	//
	//   Node operator[](const Key *) const;
	//
	// is a pure lookup. We provide a helper function
	// so that intentions can be made clear:
	const Node NodeFind(const Node &n, const Node &key);

	// The second 'N' template argument is only a dummy
	// so that we can define this w/o pulling in
	// yaml-cpp headers (it should always by YAML::Node).
	template <typename N, typename Key>
	const N NodeFind(const N &n, const Key &key)
	{
		return static_cast<const N>(n)[key];
	}
};

class IYamlSupportBase {
public:
	virtual void dumpYaml(YAML::Node &n) const = 0;

	static const int MIN_SUPPORTED_SCHEMA = 3;
	static const int MAX_SUPPORTED_SCHEMA = 3;
};


/*
 * The hierarchy of things
 */

/*!
 * An entity
 *
 * The same entity can be referenced from different places
 * in the hierarchy.
 *
 * NOTE: the strings returned by 'getName()', 'getDescription()'
 *       are only valid while a shared pointer to containing object
 *       is held.
 */
class IEntry {
public:
	/*!
	 * Return the name of this Entry
	 */
	virtual const char *getName()        const = 0;

	/*!
	 * Return the size (in bytes) of this Entry
	 */
	virtual uint64_t    getSize()        const = 0;

	/*!
	 * Return the description string (or NULL)
	 */
	virtual const char *getDescription() const = 0;

	/*!
	 * Return the poll interval (in seconds). This
	 * currently not used by CPSW proper but passed on
	 * for use by upper layers.
	 */
	virtual double      getPollSecs()    const = 0;

	/*!
	 * Test if this Entry is a Hub; return a shared pointer
	 * to itself (as a Hub) if this is the case, NULL otherwise.
	 */
	virtual Hub         isHub()          const = 0;

	/*!
	 * Dump information about this child to FILE
	 */
	virtual       void      dump(FILE*)    const = 0;

	/*!
	 * Dump information about this child to stdout
	 */
	virtual       void      dump()         const = 0;

	virtual ~IEntry() {}
};

/*!
 * A node which connects an Entry with a Hub
 */
class IChild : public virtual IEntry {
	public:
		/*!
		 * Return the 'owning' Hub of this Entry
		 * (or NULL if this Entry is at the root of
		 * the hierarchy).
		 */
		virtual       Hub       getOwner()     const = 0;

		/*!
		 * Return the number of elements (if this Child
		 * represents an array of identical Entries then
		 * getNelms() returns a number bigger than 1).
		 */
		virtual       unsigned  getNelms()     const = 0;

		virtual ~IChild() {}
};


/*!
 * Traverse the hierarchy.
 *
 * The user must implement an implementation for this
 * interface which performs any desired action on each
 * node in the hierarchy.
 *
 * The 'IPath::explore()' method accepts an IPathVisitor
 * and recurses through the hierarchy calling into
 * IPathVisitor from each visited node.
 */
class IPathVisitor {
public:
	/*!
	 * Executed before recursing into children.
	 * return 'true' to descend into children,
	 * 'false' otherwise.
	 */
	virtual bool  visitPre (ConstPath here) = 0;

	/*!
	 * Executed after recursion into children.
	 */
	virtual void  visitPost(ConstPath here) = 0;

	virtual ~IPathVisitor() {}
};

/*!
 * A IYamlFixup object may be passed to the methods which
 * load YAML files. They can fix-up the YAML tree before
 * CPSW creates its hierarchy.
 *
 * The fixup is executed on the root node (as specified
 * by 'root_name' in the loader methods)
 */
class IYamlFixup {
public:
	/*!
	 * Find a YAML node in a hierarchy of YAML::Map's
	 * while traversing merge keys.
	 * The path from the src to the destination node is
	 * given as a string separated with the separation character.
	 * E.g., find( rootNode, "children/mmio/at/UDP/port" )
     */
	static  YAML::Node findByName(const YAML::Node &src, const char *path, char sep = '/');

	/*!
	 * NOTE: When writing fixup code, be aware that the index
     *       operator 'operator[](Key &k)' creates a new map
	 *       entry if 'k' is not found.
	 *       You must use 'operator[](Key &k) const', i.e.,
	 *       do the lookup from a 'const YAML::Node' object.
	 *       You may use the 'YAML::NodeFind' template above
	 *       to make your intentions more obvious.
	 */
	virtual void operator()(YAML::Node &root, YAML::Node &top) = 0;

	virtual ~IYamlFixup() {}
};


class IPath {
public:
	// lookup 'name' under this path and return new 'full' path
	virtual Path        findByName(const char *name) const = 0;
	// strip last element off this path and return child at tail (or NULL if none)
	virtual Child       up()                               = 0;
	// test if this path is empty
	virtual bool        empty()                      const = 0;

	// return depth of the path, i.e., how many '/' separated
	// "levels" there are
	virtual int         size()                       const = 0;

	virtual void        clear()                            = 0; // absolute; reset to root
	virtual void        clear(Hub)                         = 0; // relative; reset to passed arg
	// return Hub at the tip of this path (if any -- NULL otherwise)
	virtual       Hub   origin()                     const = 0;
	// return parent Hub (if any -- NULL otherwise )
	virtual       Hub   parent()                     const = 0;
	// return Child at the end of this path (if any -- NULL otherwise)
	virtual Child       tail()                       const = 0;
	virtual std::string toString()                   const = 0;
	virtual void        dump(FILE *)                 const = 0;
	// verify the 'head' of 'p' is identical with the tail of this path
	virtual bool        verifyAtTail(ConstPath p)          = 0;
	// append a copy of another path to this one.
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual void        append(ConstPath p)                = 0;

	// append a copy of another path to a copy of this one and return the new copy
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual Path        concat(ConstPath p)          const = 0;

	// make a copy of this path
	virtual Path        clone()                      const = 0;

	// Compute a new path which is the intersection of this path and p
	// (I.e., it contains all array elements (if any) common to both
	// paths). Returns an empty path if there are no common elements.
	virtual Path        intersect(ConstPath p)       const = 0;

	// Slightly more efficient version if you only are interested
	// in whether paths intersect or not
	virtual bool        isIntersecting(ConstPath p)  const = 0;

	// count number of array elements addressed by this path
	virtual unsigned    getNelms()                   const = 0;

	// 'from' and 'to' indices addressed by the tail of this path
	virtual unsigned    getTailFrom()                const = 0;
	virtual unsigned    getTailTo()                  const = 0;

	// recurse through the hierarchy
	virtual void        explore(IPathVisitor *)      const = 0;

	/*!
	 * Recurse through hierarchy (underneath this path), and let
	 * entries dump their current values (aka "configuration")
	 * in YAML format.
	 * When this call returns then the 'template' node is the
	 * root of a hierarchy of YAML nodes which serializes the
	 * current values.
	 * And existing YAML hierarchy may be passed ('template')
	 * in which case entries are processed as defined by the 'template'.
	 * The template is populated with current values.
	 * An 'empty' (undefined or NULL) template may be passed.
	 * In this case the children of a hub are processed in an
	 * order of increasing 'configPrio' (a property defined by
	 * each entry).
	 * This helps creating a configuration file for the first
	 * time.
	 *
	 * RETURNS: number of entries saved/loaded
	 */
	virtual uint64_t    dumpConfigToYaml (YAML::Node   &tmplt)   const = 0;
	virtual uint64_t    loadConfigFromYaml(YAML::Node  &config)  const = 0;
	/*!
	 * This helper routine runs the file through CPSW's YAML preprocessor;
	 * i.e. configurations loaded with this routine may use #include, #once
	 * etc.
	 */
	virtual uint64_t    loadConfigFromYamlFile(const char* filename, const char *incdir = 0) const = 0;

	// create a path
	static  Path        create();             // absolute; starting at root
	static  Path        create(const char*);  // absolute; starting at root
	static  Path        create(Hub);          // relative; starting at passed arg

	/*!
	 * Load a hierarchy definition in YAML format from a file.
	 * The hierarchy is built from the node with name 'rootName'.
	 *
	 * Optionally, 'yamlDir' may be passed which identifies a directory
	 * where *all* yaml files reside. NULL (or empty) instructs the
	 * method to use the same directory where 'fileName' resides.
	 *
	 * The directory is relevant for included YAML files.
	 *
	 * RETURNS: Root path of the device hierarchy.
	 */
	static Path loadYamlFile(
					const char *fileName,
					const char *rootName = "root",
					const char *yamlDir  = 0,
					IYamlFixup *fixup    = 0
	);

	/*!
	 * Load a hierarchy definition in YAML format from a std::istream.
	 * The hierarchy is built from the node with name 'rootName'.
	 *
	 * Optionally, 'yamlDir' may be passed which identifies a directory
	 * where *all* yaml files reside. NULL (or empty) denotes CWD.
	 *
	 * The directory is relevant for included YAML files.
	 *
	 * RETURNS: Root path of the device hierarchy.
	 */
	static Path loadYamlStream(
					std::istream &yaml,
					const char *rootName = 0,
					const char *yamlDir  = 0,
					IYamlFixup *fixup    = 0
	);

	/*!
	 * Convenience wrapper which converts a C-style string into
	 * std::istream and uses the overloaded method (see above).
	 *
	 * RETURNS: Root path of the device hierarchy.
	 */
	static Path loadYamlStream(
					const char *yaml,
					const char *rootName = "root",
					const char *yamlDir  = 0,
					IYamlFixup *fixup    = 0
	);

	virtual ~IPath() {}
};

/*!
 * A collection of child nodes
 */
class IHub : public virtual IEntry {
public:
	/*!
	 * find all entries matching 'path' in or underneath this hub.
	 * No wildcards are supported ATM. 'matching' refers to array
	 * indices which may be used in the path.
	 *
	 * E.g., if there is an array of four devices [0-3] then the
	 * path may select just two of them:
	 *
	 *      devices[1-2]
	 *
	 * omitting explicit indices selects *all* instances of an array.
	 */
	virtual Path       findByName(const char *path) const = 0;

	/*!
	 * Find a child with 'name' in this hub.
	 */
	virtual Child      getChild(const char *name)  const = 0;

	/*!
	 * Retrieve a list of all children
	 */
	virtual Children   getChildren()               const = 0;
};

/*!
 * An Enum object is a dictionary with associates strings to numerical
 * values.
 */
class IEnum : public virtual IYamlSupportBase {
public:

	typedef std::pair< CString, uint64_t>                 Item;
	typedef std::iterator<std::input_iterator_tag, Item>  IteratorBase;

	/*!
	 * Iterator interface for Enums
	 */
	class IIterator {
	public:
		virtual IIterator    & operator++()               = 0;
		virtual Item           operator*()                = 0;
		virtual bool           operator==(IIterator *)    = 0;
		virtual               ~IIterator() {}
	};

	/*!
	 * Concrete iterator using the opaque implementation
	 * behind the interface.
	 */
	class iterator : public IteratorBase {
		private:
			char rawmem[32];

			IIterator *ifp;
		public:
			iterator(const IEnum*,bool);
			iterator(const iterator&);
			iterator();
			iterator     & operator=(const iterator&);

			iterator     & operator++()        { ++(*ifp); return *this;   }
			Item           operator*()         { return *(*ifp);           }
			bool operator==(const iterator &b) { return (*ifp)==b.ifp;     }
			bool operator!=(const iterator &b) { return ! ((*ifp)==b.ifp); }

			~iterator() { ifp->~IIterator(); }
	};

	virtual iterator begin()       const = 0;
	virtual iterator end()         const = 0;

	/*!
	 * Return the number of entries in this Enum.
	 */
	virtual unsigned getNelms()    const = 0;

	/*!
	 * Map from a string to an Enum Item (number/string pair)
	 */
	virtual Item map(const char *)       = 0;

	/*!
	 * Map back from a number to an Enum Item (number/string pair)
	 */
	virtual Item map(uint64_t)           = 0;

	virtual ~IEnum() {}
};

class IVal_Base : public virtual IEntry {
public:
	/*!
	 * Return number of elements addressed by this Val.
	 *
	 * The Path used to instantiate a Val may address an array
	 * of values. This method returns the number of array elements.
	 */
	virtual unsigned getNelms()         const = 0;

	/*!
	 * Return a copy of the Path which was used to create this Val.
	 */
	virtual Path     getPath()          const = 0;

	/*!
	 * Return a const reference of the Path which was used to
	 * create this Val. This is not a copy (more efficient if
	 * you don't intend to modify it).
	 */
	virtual ConstPath getConstPath()    const = 0;

	/*!
	 * Access Mode
	 */
	typedef enum Mode { RO = 1, WO = 2, RW = 3 } Mode;

	/*!
	 * Encodings; custom encodings start at CUSTOM, values less than CUSTOM
	 * are reserved.
	 */
	typedef enum { NONE = 0, ASCII, EBCDIC, UTF_8, UTF_16, UTF_32, ISO_8859_1 = 16,
	               ISO_8859_2, ISO_8859_3, ISO_8859_4, ISO_8859_5, ISO_8859_6, ISO_8859_7,
	               ISO_8859_8, ISO_8859_9, ISO_8859_10, ISO_8859_11, ISO_8859_13 = ISO_8859_11 + 2,
	               ISO_8859_14, ISO_8859_15, ISO_8859_16,
	               IEEE_754 = 0x0f00,
	               CUSTOM   = 0x1000 } Encoding;

	/*!
	 * Retrieve the encoding of this ScalVal (if any); if the ScalVal
	 * represents a character or character-string then the encoding
	 * information may be used by the user-application to properly interpret
	 * the string contents.
	 */
	virtual Encoding getEncoding()      const = 0;

	virtual ~IVal_Base() {}

	/*!
	 * Instantiate a 'Val_Base' interface at the endpoint identified by 'path'
	 *
	 * NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does
	 *       not support this interface.
	 */
	static Val_Base create(ConstPath path);
};

/*!
 * Base interface to integral values
 */
class IScalVal_Base : public virtual IVal_Base {
public:
	/*!
	 * Return the size in bits of this ScalVal.
	 *
	 * If the ScalVal represents an array then the return value is the size
	 * of each individual element.
	 */
	virtual uint64_t getSizeBits()      const = 0; // size in bits

	/*!
	 * Return True if this ScalVal represents a signed number.
	 *
	 * If the ScalVal is read into a wider number than its native bitSize
	 * then automatic sign-extension is performed (for signed ScalVals).
	 */
	virtual bool     isSigned()         const = 0;

	/*!
	 * Return 'Enum' object associated with this ScalVal (if any).
	 *
	 * An Enum object is a dictionary with associates strings to numerical
	 * values.
	 */
	virtual Enum     getEnum()          const = 0;

	virtual ~IScalVal_Base () {}

	/*!
	 * Instantiate a 'ScalVal_Base' interface at the endpoint identified by 'path'
	 *
	 * NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does
	 *       not support this interface.
	 */
	static ScalVal_Base create(ConstPath path);
};


/*!
 * IndexRange can be used to programmatically access a subset
 * of a ScalVal.
 *
 * Assume, e.g., that ScalVal 'v' refers to an array with 100
 * elements:
 *
 *      v = IScalVal::create( path_prefix->findByName( "something[400-499]" ) );
 *
 * If you want to read just element 456 then you can
 *
 *      IndexRange rng(56);
 *      v->getVal( buf, 1, &rng );
 *
 * IndexRange also allows for easy iteration through an array:
 * Assume you want to read in chunks of 10 elements:
 *
 *      IndexRange rng(0, 9);
 *
 *      for (unsigned i=0; i<10; i++, ++rng) { // prefix increment is more efficient
 *         v->getVal( buf, 10, &rng );
 *      }
 *
 * NOTES:
 *   - range must address a slice *within* the limits of the ScalVal itself.
 *     E.g., in the above example you could not address element 300.
 *   - IndexRange has no knowledge of ScalVal. It is the user's responsibility
 *     to ensure the entire index range falls within the ScalVal's limits
 *     (otherwise you incur an exception).
 *   - For now only the rightmost array can be sliced. Support for multiple
 *     dimensions can be added in the future.
 *
 *     E.g., if you have a ScalVal:  "container[0-3]/value[0-99]"
 *     then you can only slice 'value'.
 */
class IndexRange : public std::vector< std::pair<int,int> > {
public:
	/*!
	 * Constructor for a range of elements.
	 *
	 * Ranges are inclusive, i.e., from=3, to=7 covers 3,4,5,6,7
	 */
	IndexRange(int from, int to);
	/*!
	 * Constructor for a single element
	 */
	IndexRange(int fromto);

	/*!
	 * getters/setters
	 */
	virtual int  getFrom() const;
	virtual int  getTo()   const;
	virtual void setFrom(int);
	virtual void setTo(int);
	virtual void setFromTo(int);
	virtual void setFromTo(int from, int to);

	/*!
	 * increment: (from, to) := ( to + 1,  to + to - from + 1 )
	 */
	virtual IndexRange &operator++();

	virtual ~IndexRange(){}
};

class IAsyncIO;

typedef shared_ptr<IAsyncIO> AsyncIO;

class IAsyncIO {
public:
	// Callback with an error message or NULL (SUCCESS)
	virtual void callback(CPSWError *status) = 0;
	virtual ~IAsyncIO() {}
};

/*!
 * Read-Only interface for endpoints which support integral values.
 *
 * This interface supports reading integer values e.g., registers
 * or individual bits. It may also feature an associated map of
 * 'enum strings'. E.g., a bit with such a map attached could be
 * read as 'True' or 'False'.
 *
 * NOTE: If no write operations are required then it is preferable
 *       to use the ScalVal_RO interface (as opposed to ScalVal)
 *       since the underlying endpoint may be read-only.
 *
 * Any size (1..64 bits) is represented by a (sign-extended) uint64.
 */
class IScalVal_RO : public virtual IScalVal_Base {
public:
	/*!
	 * (possibly truncating, sign-extending) read into 64/32/16/8-bit (array)
	 *
	 * NOTE: nelms must be large enough to hold ALL values addressed by the
	 *       underlying Path. The range of indices may be reduced using the
	 *       'range' argument.
	 *
	 * NOTE: If no write operations are required then it is preferable
	 *       to use the ScalVal_RO interface (as opposed to ScalVal)
	 *       since the underlying endpoint may be read-only.
	 */
	virtual unsigned getVal(uint64_t *p, unsigned nelms = 1, IndexRange *range = 0)               = 0;
	virtual unsigned getVal(AsyncIO aio, uint64_t *p, unsigned nelms = 1, IndexRange *range = 0)  = 0;
	virtual unsigned getVal(uint32_t *p, unsigned nelms = 1, IndexRange *range = 0)               = 0;
	virtual unsigned getVal(AsyncIO aio, uint32_t *p, unsigned nelms = 1, IndexRange *range = 0)  = 0;
	virtual unsigned getVal(uint16_t *p, unsigned nelms = 1, IndexRange *range = 0)               = 0;
	virtual unsigned getVal(AsyncIO aio, uint16_t *p, unsigned nelms = 1, IndexRange *range = 0)  = 0;
	virtual unsigned getVal(uint8_t  *p, unsigned nelms = 1, IndexRange *range = 0)               = 0;
	virtual unsigned getVal(AsyncIO aio, uint8_t  *p, unsigned nelms = 1, IndexRange *range = 0)  = 0;
	/*!
	 * If the underlying element has an Enum menu attached then reading strings
	 * from the interface will cause the raw numerical values to be mapped through
	 * the Enum into strings.
	 */
	virtual unsigned getVal(CString  *p, unsigned nelms = 1, IndexRange *range = 0)               = 0;
	virtual unsigned getVal(AsyncIO aio, CString *p, unsigned nelms = 1, IndexRange *range = 0)   = 0;
	virtual ~IScalVal_RO () {}

	/*!
	 * Instantiate a 'ScalVal_RO' interface at the endpoint identified by 'path'
	 *
	 * NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does
	 *       not support this interface.
	 */
	static ScalVal_RO create(ConstPath path);
};

/*!
 * Write-only Interface (not supported)
 */
class IScalVal_WO : public virtual IScalVal_Base {
public:
	/*!
	 * (possibly truncating) write from 64/32/16/8-bit (array)
	 * NOTE: nelms must be large enough to hold ALL values addressed by the
	 *       underlying Path. The range of indices may be reduced using the
	 *       'range' argument.
	 */
	virtual unsigned setVal(uint64_t    *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint32_t    *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint16_t    *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint8_t     *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	/*!
	 * If the underlying element has an Enum menu attached then writing strings
	 * to the interface will cause these to be mapped through the Enum into raw
	 * numerical values.
	 */
	virtual unsigned setVal(const char* *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint64_t     v, IndexRange *range = 0) = 0; // set all elements to same value
	virtual unsigned setVal(const char*  v, IndexRange *range = 0) = 0; // set all elements to same value
	virtual ~IScalVal_WO () {}

	/*!
	 * Instantiate a 'ScalVal_WO' interface at the endpoint identified by 'path'
	 *
	 * NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does
	 *       not support this interface.
	 */
	static ScalVal_WO create(ConstPath path);
};

/*!
 * Read-Write Interface for endpoints which support scalar values.
 *
 * This interface supports reading/writing integer values e.g., registers
 * or individual bits. It may also feature an associated map of
 * 'enum strings'. E.g., a bit with such a map attached could be
 * read/written as 'True' or 'False'.
 */
class IScalVal: public virtual IScalVal_RO, public virtual IScalVal_WO {
public:
	virtual ~IScalVal () {}

	/*!
	 *
	 * Instantiate a 'ScalVal' interface at the endpoint identified by 'path'
	 *
	 * NOTE: an InterfaceNotImplemented exception is thrown if the endpoint does
	 *       not support this interface.
	 */
	static ScalVal create(ConstPath path);
};

// Analogous to ScalVal there could be interfaces to double, enums, strings...


// We don't want the timeout class to clutter this header.
//
/*!
 * The most important features are
 *
 *   CTimeout( uint64_t        timeout_microseconds );
 *   CTimeout( struct timespec timeout              );
 *   CTimeout( time_t          seconds              , long nanoseconds );
 *   CTimeout()
 *      the default constructor builds an INDEFINITE timeout
 *
 *  and there are operators '+=',  '-=', '<'
 */
class CTimeout;
#include <cpsw_api_timeout.h>

/*!
 * Interface for endpoints with support streaming of raw data.",
 */
class IStream : public virtual IVal_Base {
public:
	/*!
	 * Read raw bytes from a streaming interface into a buffer and return the number of bytes read.
	 *
	 * The 'timeoutUs' argument may be used to limit the time this
	 * method blocks waiting for data to arrive. A (relative) timeout
	 * in micro-seconds may be specified. TIMEOUT_INDEFINITE blocks
	 * indefinitely.
	 *
	 * 'off' bytes of data are skipped before storing into the buffer.
	 *
	 * RETURNS: number of octets read (may be less than 'size')
	 */
	virtual int64_t read(uint8_t *buf, uint64_t size,  const CTimeout timeoutUs = TIMEOUT_INDEFINITE, uint64_t off = 0) = 0;

	/*!
	 * Write raw bytes to a streaming interface and return the number of bytes written.
	 */
	virtual int64_t write(uint8_t *buf, uint64_t size, const CTimeout timeoutUs = TIMEOUT_NONE) = 0;

	/*!
	 * Instantiate a 'Stream' interface at the endpoint identified by 'path'
	 */
	static Stream create(ConstPath path);
};

/*!
 * Interface for endpoints with support for executing arbitrary commands
 */
class ICommand;
typedef shared_ptr<ICommand> Command;

class ICommand: public virtual IVal_Base {
public:
	/*!
	 * Execute the command in the context of the calling thread.
	 * The semantics are defined by the underlying implementation.
	 */
	virtual void execute()                = 0;

	/*!
	 * Instantiate a 'Command' interface at the endpoint identified by 'path'
	 */
	static Command create(ConstPath p);
};


/*!
 * Interface for endpoints with support for floating-point numbers (read-only)
 */
class IDoubleVal_RO : public virtual IVal_Base {
public:

	/*!
	 * Read values -- see ScalVal_RO::getVal().
	 */
	virtual unsigned getVal(double *p, unsigned nelms = 1, IndexRange *range = 0)              = 0;
	virtual unsigned getVal(AsyncIO aio, double *p, unsigned nelms = 1, IndexRange *range = 0) = 0;

	virtual ~IDoubleVal_RO(){}

	/*!
	 * Instantiate a 'DoubleVal_RO' interface at the endpoint identified by 'path'
	 */
	static DoubleVal_RO create(ConstPath path);
};

/*!
 * Interface for endpoints with support for floating-point numbers (write-only)
 */
class IDoubleVal_WO : public virtual IVal_Base {
public:

	/*!
	 * Write values -- see ScalVal_WO::setVal().
	 */
	virtual unsigned setVal(double    *p, unsigned nelms = 1, IndexRange *range = 0) = 0;
	virtual unsigned setVal(double     v, IndexRange *range = 0) = 0; // set all elements to same value

	virtual ~IDoubleVal_WO(){}

	/*!
	 * Instantiate a 'DoubleVal_WO' interface at the endpoint identified by 'path'
	 */
	static DoubleVal_WO create(ConstPath path);
};

/*!
 * Interface for endpoints with support for floating-point numbers (read-write)
 */
class IDoubleVal: public virtual IDoubleVal_RO, public virtual IDoubleVal_WO {
public:
	virtual ~IDoubleVal(){}

	/*!
	 * Instantiate a 'DoubleVal' interface at the endpoint identified by 'path'
	 */
	static DoubleVal create(ConstPath path);
};

/*!
 * Obtain the GIT version string of the library
 */
const char *getCPSWVersionString();

/*!
 * Set verbosity of a library component.
 * Returns 0 on success, negative value if
 * component not supported.
 *
 * If 'component' is NULL then a list of all
 * known components is printed.
 */
int setCPSWVerbosity(const char *component = 0, int value = 0);

#endif
