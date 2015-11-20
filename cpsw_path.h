#ifndef CPSW_PATH_H
#define CPSW_PATH_H

#include <list>
#include <vector>
#include <cpsw_api_user.h>
#include <cstdarg>

class IAddress;
typedef shared_ptr<const IAddress> Address;

class CDevImpl;
typedef shared_ptr<const CDevImpl> ConstDevImpl;

struct PathEntry {
	Address  c_p;
	int      idxf, idxt;
	unsigned nelmsLeft;

	PathEntry(Address a, int idxf = 0, int idxt = -1, unsigned nelmsLeft = 1);
};

typedef std::vector<PathEntry>  PathEntryContainer;

class IPathImpl : public IPath {
public:
	virtual PathEntry    tailAsPathEntry() const = 0;
	virtual ConstDevImpl originAsDevImpl() const = 0;
	virtual ConstDevImpl parentAsDevImpl() const = 0;
};

// NOTE: all paths supplied to the constructor(s) must remain valid
// and unmodified (at least from the beginning up to the node which
// was at the tail at the time this iterator was created) while an
// iterator is in use!
class CompositePathIterator : public PathEntryContainer::reverse_iterator {
	private:
		bool                                  at_end;
		std::vector<PathEntryContainer::reverse_iterator> l;
		unsigned                              nelmsRight;
		unsigned                              nelmsLeft;

	public:
		// construct from a single path 
		CompositePathIterator(Path *p);
		// construct from a NULL-terminated list of paths
		CompositePathIterator(Path *p0, Path *p, ...);

		unsigned getNelmsRight()
		{
			return nelmsRight;
		}

		unsigned getNelmsLeft()
		{
			return nelmsLeft * (atEnd() ? 1 : (*this)->nelmsLeft);
		}

		bool atEnd()
		{
			return at_end;
		}

		// can path 'p' be concatenated with this one, i.e.,
		// is the origin of 'p' identical with the element
		// this iterator points to?
		bool validConcatenation(Path p);

		void append(Path p);

		CompositePathIterator & operator++();
		CompositePathIterator   operator++(int);

		// -- not implemented; will throw an exception
		CompositePathIterator &operator--();
		CompositePathIterator operator--(int);

		void dump(FILE *f) const;
};

#endif
