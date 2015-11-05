#ifndef CPSW_PATH_H
#define CPSW_PATH_H

#include <list>
#include <api_user.h>
#include <cstdarg>

class Address;
class Dev;

struct PathEntry {
	const Child *c_p;
	int    idxf, idxt;

	PathEntry(const Child *a, int idxf = 0, int idxt = -1);
};

// NOTE: all paths supplied to the constructor(s) must remain valid
// and unmodified while an iterator is in use!
class CompositePathIterator : public std::list<PathEntry>::reverse_iterator {
	private:
		bool                                  at_end;
		// FIXME: using a list (with dynamically allocated nodes) should be revisited.
		std::list<std::list<PathEntry>::reverse_iterator> l;
		unsigned                              nelmsRight;

	public:
		unsigned getNelmsRight()
		{
			return nelmsRight;
		}

		bool atEnd()
		{
			return at_end;
		}

		bool validConcatenation(Path p);

		void append(Path p);

		// construct from a single path 
		CompositePathIterator(Path *p);
		// construct from a NULL-terminated list of paths
		CompositePathIterator(Path *p0, Path *p, ...);

		CompositePathIterator & operator++();
		CompositePathIterator operator++(int);

		// -- not implemented; will throw an exception
		CompositePathIterator &operator--();
		CompositePathIterator operator--(int);

		void dump(FILE *f) const;
};

#endif
