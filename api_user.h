#ifndef CPSW_API_USER_H
#define CPSW_API_USER_H

#include <stdint.h>
#include <list>
#include <string.h>
#include <boost/shared_ptr.hpp>

using std::list;
using boost::shared_ptr;

class Hub;
class Child;
class PathInterface;
class CompositePathIterator;
typedef shared_ptr<PathInterface>  Path;
class EventListener;

#include <cpsw_error.h>

// Read-only interface to an integral value.
// Any size (1..64 bits) is represented by
// a (sign-extended) int64_t.
class ScalVal_RO {
public:
	virtual int      getNelems()         = 0; // if array
	virtual int      getSizeBits()       = 0; // # bits per element
	virtual void     getVal(uint64_t *p) = 0; // (possibly truncating) read into 64-bit (array)
	virtual void     getVal(uint32_t *p) = 0; // (possibly truncating) read into 32-bit (array)
	virtual void     getVal(uint16_t *p) = 0; // (possibly truncating) read into 16-bit (array)
	virtual void     getVal(uint8_t  *p) = 0; // (possibly truncating) read into  8-bit (array)
	virtual bool     isSigned()          = 0;
	virtual ~ScalVal_RO () {}
};

// Write-only: not provided ATM; caching may be required anyways

// Read-write
class ScalVal : public ScalVal_RO {
public:
	virtual void     setVal(uint64_t *p) = 0; // (possibly truncating) write from 64-bit value(s)
	virtual void     setVal(uint32_t *p) = 0; // (possibly truncating) write from 32-bit value(s)
	virtual void     setVal(uint16_t *p) = 0; // (possibly truncating) write from 16-bit value(s)
	virtual void     setVal(uint8_t  *p) = 0; // (possibly truncating) write from  8-bit value(s)
	virtual void     setVal(uint64_t  v) = 0; // set all elements to same value
	virtual ~ScalVal () {}
};

// Analogous to ScalVal there could be interfaces to double, enums, strings...

// The hierarchy of things


// A node 
class Entry {
private:
	const std::string name;
	const uint64_t    sizeBits;
	
public:
	Entry(const char *name, uint64_t sizeBits);

	virtual const char *getName() const
	{
		return name.c_str();
	}

	virtual uint64_t getSizeBits() const
	{
		return sizeBits;
	}

	virtual ~Entry() {}
};

class PathInterface {
public:
	// lookup 'name' under this path and return new 'full' path
	virtual Path        findByName(const char *name) const = 0;
	// strip last element of this path
	virtual void        up()                         = 0;
	// test if this path is empty
	virtual bool        empty()                const = 0;
	virtual void        clear()                      = 0; // absolute; reset to root
	virtual void        clear(const Hub *)           = 0; // relative; reset to passed arg
	// return Hub at the tip of this path (if any -- NULL otherwise)
	virtual const Hub  *origin()               const = 0;
	// return Entry at the end of this path (if any -- NULL otherwise)
	virtual const Entry*tail()                 const = 0;
	virtual std::string toString()             const = 0;
	virtual void        dump(FILE *)           const = 0;
	// verify the 'hub' is at the tail of this path
	virtual bool        verifyAtTail(const Hub *)    = 0;
	// append a copy of another path to this one.
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual void        append(Path p)               = 0;
	// append a copy of another path to a copy of this one and return the new copy
	// Note: an exception is thrown if this->verifyAtTail( p->origin() ) evaluates to 'false'.
	virtual Path        concat(Path p)         const = 0;

	// create an empty path
	static  Path        create();             // absolute; starting at root
	static  Path        create(const Hub *);  // relative; starting at passed arg
};

// A collection of nodes
class Hub : public Entry {
public:
	Hub(const char *name, uint64_t sizeBits) : Entry(name, sizeBits) {}
	// find all entries matching 'path' in or underneath this hub
	virtual Path           findByName(const char *path)   const = 0;

	virtual const Child   *getChild(const char *name)     const = 0;

	virtual               ~Hub() {}

};

extern const Hub * const theRootHub;

// Event AKA Interrupt interface

class EventSource {
public:
	class Event {
		public:
		virtual ~Event() {};
	};
	virtual void addListener(Event *, EventListener *)    = 0;
	virtual void removeListener(Event *, EventListener *) = 0;
	virtual ~EventSource() {}
};

class EventListener {
public:
	virtual void notify(EventSource::Event *e) = 0;
	virtual ~EventListener() {}
};

#endif
