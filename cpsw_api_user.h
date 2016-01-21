#ifndef CPSW_API_USER_H
#define CPSW_API_USER_H

#include <stdint.h>
#include <list>
#include <string.h>
#include <boost/shared_ptr.hpp>
#include <vector>

using std::list;
using boost::shared_ptr;

class IEntry;
class IHub;
class IChild;
class IPath;
class IScalVal_RO;
class IScalVal_WO;
class IScalVal;
typedef shared_ptr<const IHub>   Hub;
typedef shared_ptr<const IChild> Child;
typedef shared_ptr<const IEntry> Entry;
typedef shared_ptr<IPath>        Path;
typedef shared_ptr<IScalVal_RO>  ScalVal_RO;
typedef shared_ptr<IScalVal_WO>  ScalVal_WO;
typedef shared_ptr<IScalVal>     ScalVal;
class IEventListener;
class IEventSource;
typedef shared_ptr<IEventSource> EventSource;
class IStream;
typedef shared_ptr<IStream> Stream;

typedef shared_ptr< std::vector<Child> > Children;

#include <cpsw_error.h>

// The hierarchy of things

// An entity
//
// The same entity can be referenced from different places
// in the hierarchy.

// NOTE: the strings returned by 'getName()', 'getDescription()'
//       are only valid while a shared pointer to containing object
//       is held.
class IEntry {
public:
	virtual const char *getName()        const = 0;
	virtual uint64_t    getSize()        const = 0;
	virtual const char *getDescription() const = 0;
	virtual Hub         isHub()          const = 0;
	virtual ~IEntry() {}
};

// A node which connects an entry with a hub

class IChild : public virtual IEntry {
	public:
		virtual       Hub       getOwner()     const = 0;
		virtual       unsigned  getNelms()     const = 0;
		virtual       void      dump(FILE*)    const = 0;
		virtual       void      dump()         const = 0;
		virtual ~IChild() {}
};

class IPath {
public:
	// lookup 'name' under this path and return new 'full' path
	virtual Path        findByName(const char *name) const = 0;
	// strip last element of this path and return child at tail (or NULL if none)
	virtual Child       up()                         = 0;
	// test if this path is empty
	virtual bool        empty()                const = 0;
	virtual void        clear()                      = 0; // absolute; reset to root
	virtual void        clear(Hub)                   = 0; // relative; reset to passed arg
	// return Hub at the tip of this path (if any -- NULL otherwise)
	virtual       Hub   origin()               const = 0;
	// return parent Hub (if any -- NULL otherwise )
	virtual       Hub   parent()               const = 0;
	// return Child at the end of this path (if any -- NULL otherwise)
	virtual Child       tail()                 const = 0;
	virtual std::string toString()             const = 0;
	virtual void        dump(FILE *)           const = 0;
	// verify the 'head' of 'p' is identical with the tail of this path
	virtual bool        verifyAtTail(Path p)         = 0;
	// append a copy of another path to this one.
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual void        append(Path p)               = 0;
	
	// append a copy of another path to a copy of this one and return the new copy
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual Path        concat(Path p)         const = 0;

	// make a copy of this path
	virtual Path        clone()                const = 0;

	virtual unsigned    getNelms()             const = 0;

	// create an empty path
	static  Path        create();             // absolute; starting at root
	static  Path        create(const char*);  // absolute; starting at root
	static  Path        create(Hub);          // relative; starting at passed arg

	virtual ~IPath() {}
};

// A collection of nodes
class IHub : public virtual IEntry {
public:
	// find all entries matching 'path' in or underneath this hub
	virtual Path       findByName(const char *path)      = 0;

	virtual Child      getChild(const char *name)  const = 0;

	virtual Children   getChildren()               const = 0;
};

// Base interface to integral values
class IScalVal_Base : public virtual IEntry {
public:
	virtual unsigned getNelms()               = 0; // if array
	virtual uint64_t getSizeBits()      const = 0; // size in bits
	virtual bool     isSigned()         const = 0;
	virtual Path     getPath()          const = 0;
	virtual ~IScalVal_Base () {}
};


// IndexRange can be used to programmatically access a subset
// of a ScalVal.
// Assume, e.g., that ScalVal 'v' refers to an array with 100
// elements:
//
//    v = IScalVal::create( path_prefix->findByName( "something[400-500]" ) );
//
// If you want to read just element 456 then you can
//
//    IndexRange rng(456);
//
//    v->getVal( buf, 1, &rng );
//
// IndexRange also allows for easy iteration through an array:
// Assume you want to read in chunks of 10 elements:
//
//    IndexRange rng(400, 409);
//
//    for (unsigned i=0; i<10; i++, ++rng) { // prefix increment is more efficient
//       v->getVal( buf, 10, &rng );
//    }
//
// NOTES:
//   - range must address a slice *within* the limits of the ScalVal itself.
//     E.g., in the above example you could not address element 300.
//   - IndexRange has no knowledge of ScalVal. It is the user's responsibility
//     to ensure the entire index range falls within the ScalVal's limits
//     (otherwise you incur an exception).
//   - For now only the rightmost array can be sliced. Support for multiple
//     dimensions can be added in the future.
//
//     E.g., if you have a ScalVal:  "container[0-3]/value[0-100]"
//     then you can only slice 'value'.
//
class IndexRange : public std::vector< std::pair<int,int> > {
public:
	// ranges are inclusive, i.e., from=3, to=7 covers 3,4,5,6,7
	IndexRange(int from, int to);
	IndexRange(int fromto);
	virtual int  getFrom();
	virtual int  getTo();
	virtual void setFrom(int);
	virtual void setTo(int);
	virtual void setFromTo(int);
	virtual void setFromTo(int from, int to);

	// increment: (from, to) := ( to + 1,  to + to - from + 1 )
	virtual IndexRange &operator++();

	virtual ~IndexRange(){}
};

// Read-only interface to an integral value.
// Any size (1..64 bits) is represented by
// a (sign-extended) uint.
class IScalVal_RO : public virtual IScalVal_Base {
public:
    // (possibly truncating, sign-extending) read into 64/32/16/8-bit (array)
	// NOTE: nelms must be large enough to hold ALL values addressed by the
	//       underlying Path. The range of indices may be reduced using the
	//       'range' argument.
	virtual unsigned getVal(uint64_t *p, unsigned nelms, IndexRange *range = 0)      = 0;
	virtual unsigned getVal(uint32_t *p, unsigned nelms, IndexRange *range = 0)      = 0;
	virtual unsigned getVal(uint16_t *p, unsigned nelms, IndexRange *range = 0)      = 0;
	virtual unsigned getVal(uint8_t  *p, unsigned nelms, IndexRange *range = 0)      = 0;
	virtual ~IScalVal_RO () {}

	// Throws an exception if path doesn't point to an object which supports this interface
	static ScalVal_RO create(Path p);
};

// Write-only:
class IScalVal_WO : public virtual IScalVal_Base {
public:
    // (possibly truncating) write from 64/32/16/8-bit (array)
	// NOTE: nelms must be large enough to hold ALL values addressed by the
	//       underlying Path. The range of indices may be reduced using the
	//       'range' argument.
	virtual unsigned setVal(uint64_t *p, unsigned nelms, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint32_t *p, unsigned nelms, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint16_t *p, unsigned nelms, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint8_t  *p, unsigned nelms, IndexRange *range = 0) = 0;
	virtual unsigned setVal(uint64_t  v, IndexRange *range = 0) = 0; // set all elements to same value
	virtual ~IScalVal_WO () {}

	// Throws an exception if path doesn't point to an object which supports this interface
	static ScalVal_WO create(Path p);
};

// Read-write:
class IScalVal: public virtual IScalVal_RO, public virtual IScalVal_WO {
public:
	virtual ~IScalVal () {}
	static ScalVal create(Path p);
};

// Analogous to ScalVal there could be interfaces to double, enums, strings...


// We don't want the timeout class to clutter this header.
//
// The most important features are
//
//   CTimeout( uint64_t        timeout_microseconds );
//   CTimeout( struct timespec timeout              );
//   CTimeout( time_t          seconds              , long nanoseconds );
//   CTimeout()
//      the default constructor builds an INDEFINITE timeout
//
// and there are operators '+=',  '-=', '<'
//
#include <cpsw_api_timeout.h>

static const CTimeout TIMEOUT_NONE( 0, 0 );
static const CTimeout TIMEOUT_INDEFINITE;


class IStream {
public:
	virtual int64_t read(uint8_t *buf, size_t size, CTimeout timeout = TIMEOUT_INDEFINITE, uint64_t off = 0) = 0;


	static Stream create(Path p);
};

#endif
