#ifndef CPSW_HUB_H
#define CPSW_HUB_H

#include <api_user.h>
#include <map>

#include <stdio.h>
#include <string.h>
#include <ctype.h>


class Child {
	public:
		virtual const char     *getName() const = 0;
		virtual const Entry   *getEntry() const = 0;
		virtual       unsigned getNelms() const = 0;
		virtual int   read(CompositePathIterator *node, uint8_t *dst, uint64_t off, int headBits, uint64_t sizeBits) const = 0;

};

#include <cpsw_address.h>

struct StrCmp {
    bool operator () (const char *a, const char *b ) const {
        return strcmp(a , b) < 0 ? true : false;
    }
};

class Dev : public Hub {
	private:
		typedef  std::map<const char*, Address *, StrCmp> Children;
		mutable  Children children; // only by 'add' method

	protected:
		virtual void add(Address *a, Entry *child);

	public:
		Dev(const char *name, uint64_t sizeBits = 0);
		
		// template: each (device-specific) address must be instantiated
		// by it's creator device and then added.
		virtual void addAtAddr(Entry *child, unsigned nelms = 1)
		{
			Address *a = new Address(this, nelms);
			add(a, child);
		}

		virtual Path findByName(const char *s) const;

		virtual const Child *getChild(const char *name) const;
};

#endif
