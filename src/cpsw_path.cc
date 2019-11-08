 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_path.h>
#include <cpsw_hub.h>
#include <cpsw_obj_cnt.h>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <iostream>

//#define PATH_DEBUG

#ifdef PATH_DEBUG
#include <cpsw_debug.h>
#endif

using cpsw::dynamic_pointer_cast;
using cpsw::static_pointer_cast;

typedef shared_ptr<const DevImpl::element_type> ConstDevImpl;

class CPathImpl;
typedef shared_ptr<CPathImpl> PathImpl;

DevImpl theRootDev( CShObj::create<DevImpl>("ROOT") );

DECLARE_OBJ_COUNTER( ocnt, "Path", 0 )

Dev IDev::getRootDev()
{
	return theRootDev;
}

class CPathImpl : public PathEntryContainer, public IPathImpl {
private:
	ConstDevImpl originDev_;

public:
	CPathImpl();
	CPathImpl(Hub);
	CPathImpl(DevImpl);

	CPathImpl(const CPathImpl&);

	virtual void clear();
	virtual void clear(Hub);
	virtual void clear(ConstDevImpl);

	virtual void dump(FILE *f) const;

	virtual std::string toString() const;

	virtual int size() const;

	virtual bool empty() const;

	CPathImpl::iterator begin();
	CPathImpl::const_iterator begin() const;

	virtual Path     findByName(const char *name) const;
	virtual Path     clone()                      const;
	virtual PathImpl cloneAsPathImpl()            const;

	virtual Path     intersect(ConstPath p)            const;
	virtual bool     isIntersecting(ConstPath p)       const;

	static bool hasParent(CPathImpl::const_reverse_iterator &i);
	static bool hasParent(CPathImpl::reverse_iterator &i);

	virtual Child up()
	{
	Child rval = NULLCHILD;
		if ( ! empty() ) {
			rval = back().c_p_;
			pop_back();
		}
		return rval;
	}

	virtual PathEntry tailAsPathEntry() const
	{
		if ( ! empty() )
			return back();

		return PathEntry( NULLADDR, 0, -1 );
	}

	virtual Child tail() const
	{
		PathEntry pe = tailAsPathEntry();
		return pe.c_p_;
	}

	virtual unsigned    getNelms() const
	{
		return tailAsPathEntry().nelmsLeft_;
	}

	virtual bool verifyAtTail(ConstPath p);
	virtual bool verifyAtTail(ConstDevImpl c);

	virtual void append(ConstPath p);
	virtual void append(Address);
	virtual void append(Address, int, int);

	virtual Path concat(ConstPath p) const;

	virtual ConstDevImpl parentAsDevImpl() const;

	virtual Hub parent() const;

	virtual	Hub origin() const
	{
		return originDev_;
	}

	virtual ConstDevImpl originAsDevImpl() const
	{
		return originDev_;
	}

	virtual unsigned    getTailFrom() const
	{
		if ( empty() )
			throw InvalidPathError("getTailFrom() called on an empty Path");
		return back().idxf_;
	}

	virtual unsigned    getTailTo() const
	{
		if ( empty() )
			throw InvalidPathError("getTailFrom() called on an empty Path");
		return back().idxt_;
	}

	virtual void        explore(IPathVisitor *) const;

	virtual uint64_t    processYamlConfig(YAML::Node &, bool) const;

	virtual uint64_t    dumpConfigToYaml(YAML::Node &node)    const
	{
		return processYamlConfig( node, true );
	}

	virtual uint64_t    loadConfigFromYaml(YAML::Node &node)  const
	{
		return processYamlConfig( node, false );
	}

	virtual uint64_t    loadConfigFromYamlFile(const char* filename, const char *incdir = 0) const
	{
	YAML::Node conf( CYamlFieldFactoryBase::loadPreprocessedYamlFile( filename, incdir, false ) );
		return loadConfigFromYaml( conf );
	}

	virtual ~CPathImpl();

protected:
	static  void        explore_r(struct explorer_ctxt *);
	static  void        explore_children_r(ConstDevImpl, struct explorer_ctxt *);
};

PathEntry::PathEntry(Address a, int idxf, int idxt, unsigned nelmsLeft)
: c_p_(a), idxf_(idxf), idxt_(idxt), nelmsLeft_( nelmsLeft )
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
		this->nelmsLeft_ *= idxt - idxf + 1;
	} else {
		idxt = -1;
	}
	idxf_ = idxf;
	idxt_ = idxt;
}

void CPathImpl::dump(FILE *f) const
{
std::string s = toString();
	fprintf(f, "%s", s.c_str());
}

static CPathImpl *_toPathImpl(Path p)
{
	return static_cast<CPathImpl*>( p.get() );
}

static const CPathImpl *_toConstPathImpl(ConstPath p)
{
	return static_cast<const CPathImpl*>( p.get() );
}


IPathImpl * IPathImpl::toPathImpl(Path p)
{
	return _toPathImpl(p);
}

CPathImpl::CPathImpl()
: PathEntryContainer(), originDev_(theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}

CPathImpl::~CPathImpl()
{
	--ocnt();
}

CPathImpl::CPathImpl(const CPathImpl &in)
: PathEntryContainer(in),
  originDev_(in.originDev_)
{
	++ocnt();
}

CPathImpl::CPathImpl(Hub h)
: PathEntryContainer()
{
ConstDevImpl c = dynamic_pointer_cast<ConstDevImpl::element_type, Hub::element_type>(h);

	if ( h && ! c )
		throw InternalError("ConstDevImpl not a Hub???");
	originDev_ = c ? c : theRootDev;

	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}

CPathImpl::CPathImpl(DevImpl c)
: PathEntryContainer(),
  originDev_(c ? c : theRootDev)
{
	// maintain an empty marker element so that the back iterator
	// can easily detect the end of the list
	push_back( PathEntry(NULLADDR) );
	++ocnt();
}


int CPathImpl::size() const
{
	// not counting the marker element
	return PathEntryContainer::size() - 1;
}

bool CPathImpl::empty() const
{
	return size() <= 0;
}

CPathImpl::iterator CPathImpl::begin()
{
	CPathImpl::iterator i = PathEntryContainer::begin();
	return ++i;
}

CPathImpl::const_iterator CPathImpl::begin() const
{
	CPathImpl::const_iterator i = PathEntryContainer::begin();
	return ++i;
}

void CPathImpl::clear(Hub h)
{
ConstDevImpl c = dynamic_pointer_cast< ConstDevImpl::element_type, Hub::element_type> ( h );

	if ( ! c && h )
		throw InternalError("Hub is not a DevImpl???");

	clear( c );
}

void CPathImpl::clear(ConstDevImpl c)
{
	PathEntryContainer::clear();
	push_back( PathEntry(NULLADDR) );

	originDev_ = c ? c : theRootDev;
}


void CPathImpl::clear()
{
	clear( static_pointer_cast<ConstDevImpl::element_type>( theRootDev ) );
}

bool CPathImpl::hasParent(CPathImpl::const_reverse_iterator &i)
{
	return !! i->c_p_;
}

bool CPathImpl::hasParent(CPathImpl::reverse_iterator &i)
{
	return !! i->c_p_;
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
		d    = div(i, 10);
		here = s->insert(here, d.rem + '0');
		i    = d.quot;
	} while ( i );
}

Path
CPathImpl::intersect(ConstPath p) const
{
	if ( !p || p->empty() || originAsDevImpl() != _toConstPathImpl(p)->originAsDevImpl() )
		return Path();

CPathImpl::const_iterator a  ( begin()    );
CPathImpl::const_iterator a_e( end()      );
CPathImpl::const_iterator b  ( _toConstPathImpl(p)->begin() );
CPathImpl::const_iterator b_e( _toConstPathImpl(p)->end()   );

PathImpl rval( cpsw::make_shared<CPathImpl>( originAsDevImpl() ) );


	while ( a != a_e ) {
	int      f,t;

		if ( b == b_e)
			return Path();

		// a != a_e && b != b_e
		if ( a->c_p_ != b->c_p_ )
			return Path();

		if ( b->idxf_ > (f = a->idxf_) )
			f = b->idxf_;

		if ( b->idxt_ < (t = a->idxt_) )
			t = b->idxt_;

		if ( f > t )
			return Path();

		rval->append( a->c_p_, f, t );

		++a; ++b;
	}
	return (b == b_e) ? rval : Path();
}

bool
CPathImpl::isIntersecting(ConstPath p) const
{
	if ( !p || p->empty() || originAsDevImpl() != _toConstPathImpl(p)->originAsDevImpl() )
		return false;

CPathImpl::const_iterator a  ( begin()    );
CPathImpl::const_iterator a_e( end()      );
CPathImpl::const_iterator b  ( _toConstPathImpl(p)->begin() );
CPathImpl::const_iterator b_e( _toConstPathImpl(p)->end()   );

	while ( a != a_e ) {
	int      f,t;

		if ( b == b_e)
			return false;

		// a != a_e && b != b_e
		if ( a->c_p_ != b->c_p_ )
			return false;

		if ( b->idxf_ > (f = a->idxf_) )
			f = b->idxf_;

		if ( b->idxt_ < (t = a->idxt_) )
			t = b->idxt_;

		if ( f > t )
			return false;

		++a; ++b;
	}
	return (b == b_e);
}


std::string CPathImpl::toString() const
{
std::string rval;
	for ( CPathImpl::const_iterator it=begin(); it != end(); ++it ) {
		rval.push_back('/');
		rval.append(it->c_p_->getName());
		if ( it->c_p_->getNelms() > 1 ) {
			rval.push_back('[');
			appendNum( &rval, it->idxf_ );
			if ( it->idxt_ != it->idxf_ ) {
				rval.push_back('-');
				appendNum( &rval, it->idxt_ );
			}
			rval.push_back(']');
		}
	}
	return rval;
}

Path IPath::create()
{
	return Path( cpsw::make_shared<CPathImpl>() );
}

Path IPath::create(const char *key)
{
	return Path( cpsw::make_shared<CPathImpl>() )->findByName( key );
}


Path IPath::create(Hub h)
{
	return Path( cpsw::make_shared<CPathImpl>( h ) );
}


// scan a decimal number;
// ASSUMPTIONS: from < to on entry
static inline int getnum(const char *from, const char *to)
{
unsigned long rval;
char         *endp;

	rval = strtoul(from, &endp, 0);

	if ( endp != to || rval > (unsigned long)INT_MAX )
		throw InvalidPathError( from );

	return (int)rval;
}

Path CPathImpl::findByName(const char *s) const
{
Address      found;
ConstDevImpl h;
Path         rval = cpsw::make_shared<CPathImpl>( *this );
CPathImpl    *p   = _toPathImpl( rval );
const char  *sl;

shared_ptr<const EntryImpl::element_type> tailp;

#ifdef PATH_DEBUG
CPSW::sDbg() << "checking: "<< s <<"\n";
#endif

	if ( p->empty() ) {
#ifdef PATH_DEBUG
CPSW::sDbg() << "using origin\n";
#endif
	} else {
		found = p->back().c_p_;
#ifdef PATH_DEBUG
CPSW::sDbg() <<"starting at: "<<found->getName() << "\n";
#endif
	}

	do {
		if ( found ) {
			tailp = found->getEntryImpl();
		} else if ( ! (tailp = p->originAsDevImpl()) ) {
			throw InternalError();
		}

		int      idxf  = -1;
		int      idxt  = -1;

		// find substring
		while ( '/' == *s )
			s++;

		if ( ! *s ) {
			// end of string reached; (this handles trailing slashes)
			return rval;
		}

		sl = strchr(s,'/');

		// Check for '..'
		if ( s[0] == '.' && s[1] == '.' && ( 0 == s[2] || ( sl && 2 == sl - s ) ) ) {

			/* implies s[0] != 0 , s[1] != 0 */
			if ( p->empty() ) {
				throw InvalidPathError(s);
			}
			p->pop_back();
			found = p->back().c_p_;

		} else {

			const char *op = strchr(s,'[');
			const char *cl = op ? strchr(op,']') : 0;
			const char *mi = op ? strchr(op,'-') : 0;

			if ( ! (h = tailp->isConstDevImpl()) ) {
				throw NotDevError( tailp->getName() );
			}


			if ( sl ) {
				if ( op && op > sl )
					op = 0;
				if ( cl && cl > sl )
					cl = 0;
				if ( mi && mi > sl )
					mi = 0;
			}

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
			CPSW::sDbg() << "looking for: " << key << " in: " << h->getName() << "\n";
#endif

			found = h->getAddress( key.c_str() );

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

			p->push_back( PathEntry(found, idxf, idxt, p->getNelms()) );

		}

	} while ( (s = sl) != NULL );

	return rval;
}

ConstDevImpl CPathImpl::parentAsDevImpl() const
{
	if ( empty() )
		return NULLDEVIMPL;

CPathImpl::const_reverse_iterator it = rbegin();

	++it;
	return hasParent( it ) ? cpsw::static_pointer_cast<const CDevImpl, CEntryImpl>(it->c_p_->getEntryImpl()) : NULLDEVIMPL;
}

Hub CPathImpl::parent() const
{
	return parentAsDevImpl();
}

bool CPathImpl::verifyAtTail(ConstPath p)
{
const CPathImpl *pi = _toConstPathImpl( p );
	return verifyAtTail( pi->originAsDevImpl() );
}

bool CPathImpl::verifyAtTail(ConstDevImpl h)
{
	if ( empty() ) {
		originDev_ = h;
		return true;
	}
	return (    static_pointer_cast<Entry::element_type, ConstDevImpl::element_type>( h )
	         == static_pointer_cast<Entry::element_type, EntryImpl::element_type>( back().c_p_->getEntryImpl() ) );
}


static void append2(CPathImpl *h, const CPathImpl *t)
{
	if ( ! h->verifyAtTail( t->originAsDevImpl() ) )
		throw InvalidPathError( t->toString() );

	CPathImpl::const_iterator it = t->begin();

	unsigned nelmsLeft = h->getNelms();
	for ( ;  it != t->end(); ++it ) {
		PathEntry e = *it;
		e.nelmsLeft_ *= nelmsLeft;
		h->push_back( e );
	}
}

Path CPathImpl::clone() const
{
	return cloneAsPathImpl();
}

PathImpl CPathImpl::cloneAsPathImpl() const
{
	return cpsw::make_shared<CPathImpl>( *this );
}

Path CPathImpl::concat(ConstPath p) const
{
PathImpl rval = cloneAsPathImpl();

	if ( ! p->empty() ) {
		CPathImpl       *h = rval.get();
		const CPathImpl *t = _toConstPathImpl(p);

		append2(h, t);
	}

	return rval;
}

void CPathImpl::append(ConstPath p)
{
	if ( ! p->empty() )
		append2(this, _toConstPathImpl(p));
}

void CPathImpl::append(Address a, int f, int t)
{
	if ( ! verifyAtTail( a->getOwnerAsDevImpl() ) ) {
		throw InvalidPathError( this->toString() );
	}
	push_back( PathEntry(a, f, t, getNelms()) );
}

void CPathImpl::append(Address a)
{
	append(a, 0, -1);
}


// use a context struct for stuff we can
// pass by reference to save stack space
// (recursion)
struct explorer_ctxt {
	PathImpl     here;
	IPathVisitor *visitor;
};

void
CPathImpl::explore(IPathVisitor *visitor) const
{
struct explorer_ctxt ctxt;
int                  i,f,t;
	// recurse on a copy of 'this' path
	ctxt.here    = cloneAsPathImpl();
	ctxt.visitor = visitor;

	if ( empty() ) {
		// if the path is empty then we must start
		// at the 'originDev' - we cannot look
		// at the tail of an empty path
		explore_children_r( originAsDevImpl(), &ctxt );
	} else {
		if ( tail()->isHub() ) {
			f = ctxt.here->getTailFrom();
			t = ctxt.here->getTailTo();
            Address tla = tailAsPathEntry().c_p_;
            for ( i = f; i <= t; i++ ) {
            	ctxt.here->up();
				ctxt.here->push_back( PathEntry( tla, i, i, ctxt.here->getNelms() ) );
				explore_r( &ctxt );
			}
		} else {
			explore_r( &ctxt );
		}
	}
}

void
CPathImpl::explore_children_r(ConstDevImpl d, struct explorer_ctxt *ctxt)
{
	if ( d ) {
		CDevImpl::const_iterator it( d->begin() );
		// iterate over children
		while ( it != d->end() ) {

			if ( (*it).second->isHub() ) {
				// for hubs do descend into each individual array element
				unsigned i;
				for ( i=0; i<(*it).second->getNelms(); i++ ) {
					// append a child to the 'here' path
					ctxt->here->push_back( PathEntry( (*it).second, i, i, ctxt->here->getNelms() ) );
					// explore
					explore_r( ctxt );
					// remove the child from the 'here' path
					ctxt->here->pop_back();
				}
			} else {
				// for leaf nodes do not descend into each individual array element
				ctxt->here->push_back( PathEntry( (*it).second, -1, -1, ctxt->here->getNelms()) );
				explore_r( ctxt );
				ctxt->here->pop_back();
			}
			++it;
		}
	}
}

void
CPathImpl::explore_r(struct explorer_ctxt *ctxt)
{
	if ( ctxt->visitor->visitPre( ctxt->here ) ) {
		explore_children_r( ctxt->here->back().c_p_->getEntryImpl()->isConstDevImpl(), ctxt );
	}

	ctxt->visitor->visitPost( ctxt->here );
}

uint64_t
CPathImpl::processYamlConfig(YAML::Node &template_node, bool doDump) const
{
	if ( empty() ) {
		if ( ! originDev_ ) {
			throw InvalidPathError("dumpConfigToYaml() called on an empty Path");
		}
		return originDev_->processYamlConfig( clone(), template_node, doDump );
	} else {
		return back().c_p_->getEntryImpl()->processYamlConfig( clone(), template_node, doDump );
	}
}

bool CompositePathIterator::validConcatenation(ConstPath p)
{
	if ( atEnd() )
		return true;
	if ( p->empty() )
		return false;
	return (    static_pointer_cast<Entry::element_type, Hub::element_type      >( p->origin() )
	         == static_pointer_cast<Entry::element_type, EntryImpl::element_type>( (*this)->c_p_->getEntryImpl() ) );
}

static CPathImpl::const_reverse_iterator rbegin(ConstPath p)
{
	return _toConstPathImpl(p)->rbegin();
}


CompositePathIterator & CompositePathIterator::append(ConstPath p)
{
	if ( p->empty() )
		return *this;
	if ( ! validConcatenation( p ) ) {
		throw InvalidPathError( p->toString() );
	}
	if ( ! atEnd() ) {
		l_.push_back( *this );
		nelmsLeft_ *= (*this)->nelmsLeft_;
	} else {
		if ( nelmsLeft_ != 1 )
			throw InternalError("assertion failed: nelmsLeft should == 1 at this point");
	}
	CPathImpl::const_reverse_iterator::operator=( rbegin(p) );
	at_end_     = false;
	nelmsRight_ = 1;
	return *this;
}

CompositePathIterator::CompositePathIterator(ConstPath p)
{
	nelmsLeft_  = 1;
	if ( ! (at_end_ = p->empty()) ) {
		CPathImpl::const_reverse_iterator::operator=( rbegin( p ) );
	}
	nelmsRight_ = 1;
}

CompositePathIterator & CompositePathIterator::operator++()
{
	nelmsRight_ *= ((*this)->idxt_ - (*this)->idxf_) + 1;
	CPathImpl::const_reverse_iterator::operator++();
	if ( ! CPathImpl::hasParent(*this) ) {
		if ( ! l_.empty() ) {
			CPathImpl::const_reverse_iterator::operator=(l_.back());
			l_.pop_back();
			nelmsLeft_ /= (*this)->nelmsLeft_;
		} else {
			at_end_ = true;
			if ( nelmsLeft_ != 1 ) {
				throw InternalError("assertion failed: nelmsLeft should equal 1");
			}
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
		fprintf(f, "%s[%i-%i]<", tmp->c_p_->getName(), tmp->idxf_, tmp->idxt_);
		++tmp;
	}
	fprintf(f, "\n");
}
