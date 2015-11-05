#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <api_user.h>
#include <map>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef enum ByteOrder { UNKNOWN = 0, LE = 12, BE = 21 } ByteOrder;

ByteOrder hostByteOrder();

class Visitor;

class Visitable {
public:
	virtual void accept(Visitor *v, bool depthFirst) const = 0;
	virtual ~Visitable();
};

class Entry: public virtual IEntry {
private:
	const std::string	name;
	const uint64_t    	size;
	mutable bool        cacheable;
	mutable bool        cacheableSet;
	mutable bool        locked;

public:
	Entry(const char *name, uint64_t size);

	virtual const char *getName() const
	{
		return name.c_str();
	}

	virtual uint64_t getSize() const
	{
		return size;
	}

	virtual bool getCacheable() const
	{
		return cacheable;
	}

	virtual bool getCacheableSet() const
	{
		return cacheableSet;
	}

	// may throw exception if modified after
	// being attached
	virtual void setCacheable(bool cacheable) const;

	virtual ~Entry() {}

	void setLocked()
	{
		locked = true;
	}

	virtual void accept(Visitor *v, bool depthFirst) const;
};

class IntEntry : public Entry {
private:
	bool     is_signed;
	int      ls_bit;
	uint64_t size_bits;
public:
	IntEntry(const char *name, uint64_t sizeBits, bool is_signed, int lsBit = 0)
	: Entry(name, (sizeBits + lsBit + 7)/8), is_signed(is_signed), ls_bit(lsBit), size_bits(sizeBits)
	{
	}

	bool isSigned() const { return is_signed; }
	int  getLsBit() const { return ls_bit;    }
	uint64_t getSizeBits() const { return size_bits; }
};

class Child {
	public:
		virtual const IDev    *getOwner()     const = 0;
		virtual const char     *getName()     const = 0;
		virtual const Entry   *getEntry()     const = 0;
		virtual       unsigned getNelms()     const = 0;
		virtual ByteOrder  getByteOrder()     const = 0;
		virtual uint64_t       read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const = 0;
		virtual ~Child() {}
};


#include <cpsw_address.h>

struct StrCmp {
    bool operator () (const char *a, const char *b ) const {
        return strcmp(a , b) < 0 ? true : false;
    }
};

class Dev : public IDev, public Entry {
	private:
		typedef  std::map<const char*, Address *, StrCmp> Children;
		mutable  Children children; // only by 'add' method

	protected:
		virtual void add(Address *a, Entry *child);

	public:
		Dev(const char *name, uint64_t size= 0);
		virtual ~Dev();
		
		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddr(Entry *child, unsigned nelms = 1)
		{
			Address *a = new Address(this, nelms);
			add(a, child);
		}

		virtual Path findByName(const char *s) const;

		virtual const Child *getChild(const char *name) const;
	
		virtual void accept(Visitor *v, bool depthFirst) const;
};

class Visitor {
public:
	virtual void visit(const Entry * e)  { visit( (const Visitable *) e ); }
	virtual void visit(const Dev   * d)  { visit( (const Entry*) d );      }
	virtual void visit(const Visitable *){ throw InternalError("Unimplemented Visitor"); }
	virtual ~Visitor() {}
};

#endif
