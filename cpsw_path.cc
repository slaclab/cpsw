#include <cpsw_path.h>
#include <cpsw_hub.h>
#include <string>

#include <stdio.h>

const Dev * const theRootDev = new Dev("ROOT", 0);

class PathImpl : public std::list<PathEntry>, public IPath {
private:
	const IDev *originDev;

public:
	PathImpl();
	PathImpl(const IDev *);

	virtual void clear();
	virtual void clear(const IDev *);

	virtual void dump(FILE *f) const;

	virtual std::string toString() const;

	virtual int size() const;

	virtual bool empty() const;

	PathImpl::iterator &begin();
	PathImpl::const_iterator &begin() const;

	virtual Path findByName(const char *name) const;

	static bool hasParent(PathImpl::const_reverse_iterator &i);
	static bool hasParent(PathImpl::reverse_iterator &i);

	virtual void up() 
	{
		if ( ! empty() ) {
			pop_back();
		}
	}

	virtual bool verifyAtTail(const IDev *h);

	virtual void append(Path p);

	virtual Path concat(Path p) const;

	virtual const IEntry *tail() const;

	virtual const IDev *parent() const;

	virtual	const IDev *origin() const
	{
		return originDev;
	}
};


void PathImpl::dump(FILE *f) const
{
std::string s = toString();
	fprintf(f, "%s", s.c_str());
}

static PathImpl * toPathImpl(Path p)
{
	return static_cast<PathImpl*>( p.get() );
}

PathImpl::PathImpl() : std::list<PathEntry>(), originDev(theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(0) );
}

PathImpl::PathImpl(const IDev *h) : std::list<PathEntry>(), originDev(h ? h : theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(0) );
}


int PathImpl::size() const
{
	// not counting the marker element
	return std::list<PathEntry>::size() - 1;
}

bool PathImpl::empty() const
{
	return size() <= 0;
}

PathImpl::iterator & PathImpl::begin()
{
	PathImpl::iterator i = std::list<PathEntry>::begin();
	return ++i;
}

PathImpl::const_iterator & PathImpl::begin() const
{
	PathImpl::const_iterator i = std::list<PathEntry>::begin();
	return ++i;
}

void PathImpl::clear(const IDev *h)
{
	std::list<PathEntry>::clear();
	push_back( PathEntry(0) );
	originDev = h ? h : theRootDev;
}

void PathImpl::clear()
{
	clear( theRootDev );
}

bool PathImpl::hasParent(PathImpl::const_reverse_iterator &i)
{
	return i->c_p != NULL;
}

bool PathImpl::hasParent(PathImpl::reverse_iterator &i)
{
	return i->c_p != NULL;
}

static void appendNum(std::string *s, int i)
{
div_t d;
	if ( i < 0 ) {
		s->push_back('-');
		i = -i;
	}
	std::string::iterator here = s->end();
	do { 
		d = div(i, 10);
		s->insert(here, d.rem + '0');
		i = d.quot;
	} while ( i );
}

std::string PathImpl::toString() const
{
std::string rval;
	for ( PathImpl::const_iterator it=begin(); it != end(); ++it ) {
		rval.push_back('/');
		rval.append(it->c_p->getName());
		if ( it->c_p->getNelms() > 1 ) {
			rval.push_back('[');
			appendNum( &rval, it->idxf );
			if ( it->idxt != it->idxf ) {
				rval.push_back('-');
				appendNum( &rval, it->idxt );
			}
			rval.push_back(']');
		}
	}
	return rval;
}

Path IPath::create()
{
	return Path( new PathImpl() );
}

Path IPath::create(const IDev *h)
{
	return Path( new PathImpl( h ) );
}


// scan a decimal number;
// ASSUMPTIONS: from < to on entry
static inline int getnum(const char *from, const char *to)
{
int rval;
	for ( rval = 0; from < to; from++ ) {
			if ( '0' > *from || '9' < *from )
				throw InvalidPathError( from );
			rval = 10*rval + (*from-'0');
	}
	return rval;
}

Path PathImpl::findByName(const char *s) const
{
const Child *found;
const IDev   *h;
PathImpl    *p = new PathImpl( *this );
const char  *sl;

	if ( empty() ) {
		if ( (h = origin()) ) {
			goto use_origin;
		} else {
			throw InternalError();
		} 
	} else {
		found = back().c_p;
	}

	do {

		if ( ! (h = dynamic_cast<const IDev*>( found->getEntry() )) ) {
			throw NotDevError( found->getName() );
		}

use_origin:

		int      idxf  = -1;
		int      idxt  = -1;

		// find substring
		while ( '/' == *s )
			s++;

		const char *op = strchr(s,'[');
		const char *cl = op ? strchr(op,']') : 0;
		const char *mi = op ? strchr(op,'-') : 0;

		sl = strchr(s,'/');

		if ( op && sl && sl < op )
			op = 0;

		if ( op ) {

			if (   !cl
					|| (  cl <= op + 1)
					|| (  sl && (sl < cl || sl != cl+1))
					|| ( !sl && *(cl+1) )
					|| (  mi && (mi >= cl-1 || mi <= op + 1) ) 
			   ) {
				throw InvalidPathError( s );
			}

			idxf = getnum( op+1, mi ? mi : cl );
			if ( mi ) {
				idxt = getnum( mi+1, cl );
			}
		}

		std::string key(s, (op ? op - s : ( sl ? sl - s : strlen(s) ) ) );

		found = h->getChild( key.c_str() );

		if ( ! found ) {
			throw NotFoundError( key.c_str() );
		}

		if ( idxt < 0 )
			idxt = idxf < 0 ? found->getNelms() - 1 : idxf;
		if ( idxf < 0 )
			idxf = 0;

		if ( idxf > idxt || idxt >= found->getNelms() ) {
			throw InvalidPathError( s );
		}

		p->push_back( PathEntry(found, idxf, idxt) );

	} while ( s = sl );

	return Path(p);
}

const IEntry * PathImpl::tail() const
{
	if ( empty() )
		return originDev;
	return back().c_p->getEntry();
}

const IDev *PathImpl::parent() const
{

	if ( empty() )
		return NULL;

PathImpl::const_reverse_iterator it = rend();
	++it; // rend points after last el
	++it; // if empty this points at the NULL marker element
	return hasParent( it ) ? dynamic_cast<const IDev *>(it->c_p) : NULL;
}

bool PathImpl::verifyAtTail(const IDev *h)
{
	if ( empty() ) {
		originDev = h;
		return true;
	} 
	return static_cast<const IEntry*>(h) == back().c_p->getEntry();
}

static void append2(PathImpl *h, PathImpl *t)
{
	if ( ! h->verifyAtTail( t->origin() ) )
		throw InvalidPathError(Path(t));

	PathImpl::iterator it = t->begin();

	for ( ++it /* skip marker */;  it != t->end(); ++it ) {
		h->push_back( *it );
	}
}

Path PathImpl::concat(Path p) const
{
PathImpl *h = new PathImpl( *this );
PathImpl *t = toPathImpl(p);

	append2(h, t);

Path rval = Path( h );

	return rval;
}

void PathImpl::append(Path p)
{
	append2(this, toPathImpl(p));
}


bool CompositePathIterator::validConcatenation(Path p)
{
	if ( atEnd() )
		return true;
	if ( p->empty() )
		return false;
	return ( (*this)->c_p->getEntry() == static_cast<const IEntry*>( p->origin() ) );
}

static PathImpl::reverse_iterator rbegin(Path p)
{
	return toPathImpl(p)->rbegin();
}


void CompositePathIterator::append(Path p)
{
	if ( p->empty() )
		return;
	if ( ! validConcatenation( p ) ) {
		throw InvalidPathError(p);
	}
	if ( ! atEnd() )
		l.push_back( *this );
	PathImpl::reverse_iterator::operator=( rbegin(p) );
	at_end     = false;
	nelmsRight = 1;
}

CompositePathIterator::CompositePathIterator(Path *p0, Path *p, ...)
{
	va_list ap;
	va_start(ap, p);
	if ( ! (at_end = (*p0)->empty()) ) {
		PathImpl::reverse_iterator::operator=( rbegin(*p0) );
	}
	while ( p ) {
		append( *p );
		p = va_arg(ap, Path*);
	}
	va_end(ap);
	nelmsRight = 1;
}

CompositePathIterator::CompositePathIterator(Path *p)
{
	if ( ! (at_end = (*p)->empty()) ) {
		PathImpl::reverse_iterator::operator=( rbegin(*p) );
	}
	nelmsRight = 1;
}

CompositePathIterator & CompositePathIterator::operator++()
{
	nelmsRight *= ((*this)->idxt - (*this)->idxf) + 1;
	PathImpl::reverse_iterator::operator++();
	if ( ! PathImpl::hasParent(*this) ) {
		if ( ! l.empty() ) {
			PathImpl::reverse_iterator::operator=(l.back());
			l.pop_back();
		} else {
			at_end = true;
		}
	}
	return *this;
}

CompositePathIterator CompositePathIterator::operator++(int)
{
	CompositePathIterator tmp = *this;
	operator++();
	return tmp;
}

CompositePathIterator & CompositePathIterator::operator--()
{
	throw InternalError();
}

CompositePathIterator CompositePathIterator::operator--(int)
{
	throw InternalError();
}

void CompositePathIterator::dump(FILE *f) const
{
CompositePathIterator tmp = *this;
	while ( ! tmp.atEnd() ) {
		fprintf(f, "%s<", tmp->c_p->getName());
		++tmp;
	}
	fprintf(f, "\n");
}
