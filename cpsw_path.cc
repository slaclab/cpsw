#include <cpsw_path.h>
#include <cpsw_hub.h>
#include <string>

#include <stdio.h>
#include <iostream>

#undef PATH_DEBUG

using boost::dynamic_pointer_cast;
using std::cout;

Hub const theRootDev( EntryImpl::create<DevImpl>("ROOT") );

class PathImpl : public std::list<PathEntry>, public IPath {
private:
	Hub originDev;

public:
	PathImpl();
	PathImpl(Hub);
#define HAVE_CC
#ifdef HAVE_CC
	PathImpl(const PathImpl&);
#endif

	virtual void clear();
	virtual void clear(Hub);

	virtual void dump(FILE *f) const;

	virtual std::string toString() const;

	virtual int size() const;

	virtual bool empty() const;

	PathImpl::iterator &begin();
	PathImpl::const_iterator &begin() const;

	virtual Path findByName(const char *name) const;

	static bool hasParent(PathImpl::const_reverse_iterator &i);
	static bool hasParent(PathImpl::reverse_iterator &i);

	virtual Child up() 
	{
	Child rval = NULLCHILD;
		if ( ! empty() ) {
			rval = back().c_p;
			pop_back();
		}
		return rval;
	}

	virtual Child tail() const
	{
		if ( ! empty() )
			return back().c_p;
		return NULLCHILD;
	}

	virtual bool verifyAtTail(Hub h);

	virtual void append(Path p);
	virtual void append(Child);
	virtual void append(Child, int, int);

	virtual Path concat(Path p) const;

	virtual Hub parent() const;

	virtual	Hub origin() const
	{
		return originDev;
	}

	virtual ~PathImpl();
};

PathEntry::PathEntry(const Child a, int idxf, int idxt) : c_p(a), idxf(idxf), idxt(idxt)
{
	if ( idxf < 0 )
		idxf = 0;
	if ( a ) {
		int n = a->getNelms() - 1;
		if ( idxf > n )
			idxf = n;
		if ( idxt < 0 || idxt > n )
			idxt = n;
		if ( idxt < idxf )
			idxt = idxf;
	}
}

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
	push_back( PathEntry(NULLCHILD) );
#ifdef HAVE_CC
	cpsw_obj_count++;
#endif
}

PathImpl::~PathImpl()
{
#ifdef HAVE_CC
	cpsw_obj_count--;
#endif
}

#ifdef HAVE_CC
PathImpl::PathImpl(const PathImpl &in)
: std::list<PathEntry>(in), originDev(in.originDev)
{
	cpsw_obj_count++;
}
#endif

PathImpl::PathImpl(Hub h) : std::list<PathEntry>(), originDev(h ? h : theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLCHILD) );
	cpsw_obj_count++;
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

void PathImpl::clear(Hub h)
{
	std::list<PathEntry>::clear();
	push_back( PathEntry(NULLCHILD) );
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
	return Path( make_shared<PathImpl>() );
}

Path IPath::create(Hub h)
{
	return Path( make_shared<PathImpl>( h ) );
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
Child found;
Hub h;
Path         rval = make_shared<PathImpl>( *this );
PathImpl    *p    = toPathImpl( rval );
const char  *sl;

#ifdef PATH_DEBUG
cout<<"checking: "<< s <<"\n";
#endif

	if ( empty() ) {
		if ( (h = origin()) ) {
#ifdef PATH_DEBUG
cout<<"using origin\n";
#endif
			goto use_origin;
		} else {
			throw InternalError();
		} 
	} else {
		found = back().c_p;
#ifdef PATH_DEBUG
cout<<"starting at: "<<found->getName() << "\n";
#endif
	}

	do {

		if ( ! (h = dynamic_pointer_cast<IDev, IEntry>( found->getEntry() )) ) {
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

#ifdef PATH_DEBUG
cout<<"looking for: " << key << " in: " << h->getName() << "\n";
#endif

		found = h->getChild( key.c_str() );

		if ( ! found ) {
			throw NotFoundError( key.c_str() );
		}

		if ( idxt < 0 )
			idxt = idxf < 0 ? found->getNelms() - 1 : idxf;
		if ( idxf < 0 )
			idxf = 0;

		if ( idxf > idxt || idxt >= (int)found->getNelms() ) {
			throw InvalidPathError( s );
		}

		p->push_back( PathEntry(found, idxf, idxt) );

	} while ( (s = sl) != NULL );

	return rval;
}

Hub PathImpl::parent() const
{

	if ( empty() )
		return NULLHUB;

PathImpl::const_reverse_iterator it = rend();
	++it; // rend points after last el
	++it; // if empty this points at the NULL marker element
	return hasParent( it ) ? boost::dynamic_pointer_cast<const IHub, const IChild>(it->c_p) : NULLHUB;
}

bool PathImpl::verifyAtTail(Hub h)
{
	if ( empty() ) {
		originDev = h;
		return true;
	} 
	return (void*)h.get() == (void*)back().c_p->getEntry().get();
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
Path rval = make_shared<PathImpl>( *this );
PathImpl *h = toPathImpl(rval);
PathImpl *t = toPathImpl(p);

	append2(h, t);

	return rval;
}

void PathImpl::append(Path p)
{
	append2(this, toPathImpl(p));
}

void PathImpl::append(Child c, int f, int t)
{
	if ( ! verifyAtTail( c->getOwner() ) ) {
		throw InvalidPathError( Path( this ) );
	}
	push_back( PathEntry(c, f, t) );
}

void PathImpl::append(Child c)
{
	append(c, 0, -1);
}


bool CompositePathIterator::validConcatenation(Path p)
{
	if ( atEnd() )
		return true;
	if ( p->empty() )
		return false;
	return ( (void*)(*this)->c_p->getEntry().get() == (void*)p->origin().get() );
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
