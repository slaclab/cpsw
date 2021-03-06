 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

// This silences the 'auto_ptr deprecated' warnings.
// See https://github.com/boostorg/python/issues/176
#define BOOST_NO_AUTO_PTR

#include <cpsw_api_user.h>
#include <cpsw_python.h>
#include <cpsw_path.h>
#include <cpsw_yaml.h>
#include <yaml-cpp/yaml.h>
#include <cpsw_rssi.h>
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/list.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <cpsw_debug.h>

using cpsw::static_pointer_cast;

using namespace boost::python;
using cpsw::weak_ptr;

namespace cpsw_python {

// Translate CPSW Errors/exceptions into python exceptions
//
// This is complicated by the fact that there is no easy
// way to use 'boost::python': the problem is that exceptions
// must be derived from PyErr_Exception (C-API) and AFAIK
// there is no way to tell the boost::python 'class_' template
// that a class is derived from a python C-API object; only
// c++ classes can be bases of class_''.
// If the python object is not a subclass of PyErr_Exception
// then
//
//   try:
//     function() #raises c++ exception
//   except ClassGeneratedBy_class_template as e:
//     handle(e)
//
// works fine if indeed the expected exception is thrown.
// However, if any *other* (wrapped) exception is thrown then python
// complains:
//
//   SystemError: 'finally' pops bad exception
//
// probably because the class object that was thrown is not
// a PyErr_Exception.
//
// Thus, we must resort to some C-API Kludgery for dealing with
// CPSW Exceptions...
//
// See also:
//
// http://cplusplus-sig.plu5pluscp.narkive.com/uByjeyti/exception-translation-to-python-classes
//
// The first argument to PyErr_NewException must be a 'type' object which is
// derived from PyErr_Exception; AFAIK, boost::python does not support that yet.
// Thus, we must craft such an object ourselves...


template <typename ECL>
class ExceptionTranslator {
private:
	std::string  name_;
	PyObject    *excTypeObj_;

	ExceptionTranslator &operator=(const ExceptionTranslator &orig);

public:

	ExceptionTranslator(const ExceptionTranslator &orig)
	: name_(orig.name_),
	  excTypeObj_(orig.excTypeObj_)
	{
		if ( excTypeObj_ )
			Py_INCREF(excTypeObj_);
	}

	static bool firstTime()
	{
	static bool firstTime_ = true;
		bool rval  = firstTime_;
		firstTime_ = false;
		return rval;
	}

	// obtain exception type (C-API) object - WITHOUT incrementing
	// the reference count.
	PyObject *getTypeObj() const
	{
		return excTypeObj_;
	}

	ExceptionTranslator(const char *name, PyObject *base=0)
	: name_(name),
	  excTypeObj_(0)
	{
		if ( ! firstTime() )
			throw std::exception();

		std::string scopeName = extract<std::string>(scope().attr("__name__"));

		char qualifiedName[ scopeName.size() + name_.size() + 1 + 1 ];
		::strcpy(qualifiedName, scopeName.c_str());
		::strcat(qualifiedName, ".");
		::strcat(qualifiedName, name_.c_str());

		// create a new C-API class which is derived from 'base'
		// (must be a subclass of PyErr_Exception)
		excTypeObj_ = PyErr_NewException( qualifiedName, base, 0 );

		//CPSW::cdbg() << qualifiedName << " typeObj_ refcnt " << Py_REFCNT( excTypeObj_ ) << "\n";

		// Register in the current scope
		scope current;

		current.attr(name_.c_str()) = object( handle<>( borrowed( excTypeObj_ ) ) ); \

	//CPSW::cdbg() << "EXCEPTION TYPE REFCOUNT CRE " << Py_REFCNT(getTypeObj()) << "\n";
	}

	~ExceptionTranslator()
	{
		//CPSW::cdbg() << "~" << name_ << " typeObj_ refcnt " << Py_REFCNT( getTypeObj() ) << "\n";
		if ( excTypeObj_ )
			Py_DECREF( excTypeObj_ );
	}

	// The actual translation
	void operator() (const ECL &e) const
	{
	//CPSW::cdbg() << "EXCEPTION TYPE REFCOUNT PRE " << Py_REFCNT(getTypeObj()) << "\n";
		PyErr_SetString( getTypeObj(), e.what() );
	//CPSW::cdbg() << "EXCEPTION TYPE REFCOUNT PST " << Py_REFCNT(getTypeObj()) << "\n";
	}

// Macros to save typing
#define ExceptionTranslatorInstall(clazz) \
	ExceptionTranslator<clazz> tr_##clazz(#clazz); \
	register_exception_translator<clazz>( tr_##clazz );

#define ExceptionTranslatorInstallDerived(clazz, base) \
	ExceptionTranslator<clazz> tr_##clazz(#clazz, tr_##base.getTypeObj()); \
	register_exception_translator<clazz>( tr_##clazz );
};

typedef CGetValWrapperContextTmpl<boost::python::object, boost::python::list> CGetValWrapperContext;

// Need wrappers for methods which take
// shared pointers to const objects which
// do not seem to be handled correctly
// by boost::python.
static void wrap_Path_clear(Path p, shared_ptr<IHub> h)
{
Hub hc(h);
	p->clear(hc);
}

static Path wrap_Path_create(shared_ptr<IHub> h)
{
Hub hc(h);
	return IPath::create(hc);
}

static boost::python::list wrap_Enum_getItems(Enum enm)
{
boost::python::list l;

IEnum::iterator it  = enm->begin();
IEnum::iterator ite = enm->end();
	while ( it != ite ) {
		l.append( boost::python::make_tuple( *(*it).first, (*it).second ) );
		++it;
	}

	return l;
}

static object wrap_Val_Base_getEncoding(shared_ptr<IVal_Base> val)
{
	IVal_Base::Encoding enc = val->getEncoding();
	if ( IVal_Base::NONE == enc )
		return object();

	return object( YAML::convert<IVal_Base::Encoding>::do_encode( enc ) );
}

// Read into an object which implements the (new) python buffer interface
// (supporting only a contiguous buffer)

static unsigned wrap_ScalVal_RO_getVal_into(ScalVal_RO val, object &o, int from, int to)
{
PyObject  *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
	return IScalVal_RO_getVal( val.get(), op, from, to );
}

class IPyAsyncIO {
public:
	virtual void py_callback(boost::python::object) = 0;

	~IPyAsyncIO()
	{
	}
};

class CAsyncGetValWrapperContext : public CGetValWrapperContext, public IAsyncIO, public IPyAsyncIO {
private:
	PyObject *self_;

public:
	CAsyncGetValWrapperContext(PyObject *self)
	: self_( self )
	{
	}

	virtual void
	py_callback(boost::python::object arg)
	{
		call_method<void>(self_, "callback", arg );
	}

	virtual void callback(CPSWError *err)
	{
		PyGILState_STATE state_ = PyGILState_Ensure();
		/* Call into Python */

		py_callback( complete( err ) );

		PyGILState_Release( state_ );

	}

	~CAsyncGetValWrapperContext()
	{
	}
};

typedef shared_ptr<CAsyncGetValWrapperContext> AsyncGetValWrapperContext;

static boost::python::object wrap_ScalVal_RO_getValAsync(ScalVal_RO val, AsyncGetValWrapperContext ctxt, int from, int to, bool forceNumeric)
{
	unsigned rval = ctxt->issueGetVal( val.get(), from, to, forceNumeric, ctxt );

	return boost::python::object( rval );
}


static boost::python::object wrap_ScalVal_RO_getVal(ScalVal_RO val, int from, int to, bool forceNumeric)
{
	CGetValWrapperContext ctxt;

	ctxt.issueGetVal( val.get(), from, to, forceNumeric, AsyncIO() );

	return ctxt.complete( 0 );
}

static unsigned wrap_ScalVal_setVal(ScalVal val, object &o, int from, int to)
{
PyObject  *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
	return IScalVal_setVal( val.get(), op, from, to );
}

static int64_t
wrap_Stream_read(Stream val, object &o, int64_t timeoutUs, uint64_t offset)
{
	// no need for incrementing the refcnt while 'o' is alive
	return IStream_read( val.get(), o.ptr(), timeoutUs, offset );
}


static int64_t
wrap_Stream_write(Stream val, object &o, int64_t timeoutUs)
{
	// no need for incrementing the refcnt while 'o' is alive
	return IStream_write( val.get(), o.ptr(), timeoutUs );
}

// CPSW Streams are (implicitly) opened/closed during construction/destruction.
// Because python does not make any guarantees when destruction actually happens
// there could be a problem: as long a a stream is open, CPSW delivers data to it
// and possibly blocks if the data are never read.
// If multiple streams or other protocols share a common channel (e.g., RSSI) then
// stalling a stream might stall all other protocols, too.
// Therefore it is imperative that a Stream is destroyed in a controlled fashion.
// In python this is only possible via a context manager.
// Hence we introduce a StreamMgr wrapper class (which, alas, must delegate all
// member access via some trampoline code :-()
// The underlying interface is created/destroyed from __enter__ and __exit,
// respectively.

class CStreamMgr;

typedef shared_ptr<CStreamMgr> StreamMgr;

class CStreamMgr : public virtual IStream, boost::noncopyable {
private:
	Path                 p_;
	Stream               theStream_;
	weak_ptr<CStreamMgr> self_;

	CStreamMgr(Path p)
	: p_( p->clone() )
	{
	}

	Stream s() const
	{
		if ( theStream_ )
			return theStream_;
		throw FailedStreamError("Stream can only be used from the block of a 'with' statement!");
	}

public:

	StreamMgr
	__enter__()
	{
		theStream_ = IStream::create( p_ );
		return StreamMgr( self_ );
	}

	bool
	__exit__(
		boost::python::object &exc_type,
		boost::python::object &exc_value,
		boost::python::object &traceback)
	{
		theStream_.reset(); // closes the stream
		return false; // re-raise exception
	}

	// now there follows a lot of ugly boiler-plate code :-(

	// IEntry
	virtual const char *getName()        const
	{
		return s()->getName();
	}

	virtual uint64_t    getSize()        const
	{
		return s()->getSize();
	}

	virtual const char *getDescription() const
	{
		return s()->getDescription();
	}

	virtual double      getPollSecs()    const
	{
		return s()->getPollSecs();
	}

	virtual Hub         isHub()          const
	{
		return s()->isHub();
	}

	virtual unsigned     getNelms()      const
	{
		return s()->getNelms();
	}

	virtual void         dump(FILE *f)   const
	{
		return s()->dump( f );
	}

	virtual       void      dump()       const
	{
		return s()->dump();
	}

	// IVal_Base
	virtual unsigned getNelms()
	{
		return s()->getNelms();
	}

	virtual IVal_Base::Encoding getEncoding() const
	{
		return s()->getEncoding();
	}

	// getPath/getConstPath also should work w/o an open stream
	virtual Path     getPath()          const
	{
		return theStream_ ? theStream_->getPath() : p_->clone();
	}

	virtual ConstPath getConstPath()    const
	{
		return theStream_ ? theStream_->getConstPath() : p_;
	}

	// IStream
	virtual int64_t read(uint8_t *buf, uint64_t size,  const CTimeout timeoutUs, uint64_t off)
	{
		return s()->read( buf, size, timeoutUs, off );
	}

	virtual int64_t write(uint8_t *buf, uint64_t size, const CTimeout timeoutUs)
	{
		return s()->write( buf, size, timeoutUs );
	}

	static StreamMgr create(Path path);
};

StreamMgr
CStreamMgr::create(Path path)
{
StreamMgr theMgr  = StreamMgr( new CStreamMgr( path ) );
	theMgr->self_ = theMgr;
	return theMgr;
}

static boost::python::object wrap_DoubleVal_RO_getValAsync(DoubleVal_RO val, AsyncGetValWrapperContext ctxt, int from, int to)
{
unsigned rval = ctxt->issueGetVal( val.get(), from, to, ctxt );

	return boost::python::object( rval );
}


static boost::python::object wrap_DoubleVal_RO_getVal(DoubleVal_RO val, int from, int to)
{
	CGetValWrapperContext ctxt;

	ctxt.issueGetVal( val.get(), from, to, AsyncIO() );

	return ctxt.complete( 0 );
}

static unsigned wrap_DoubleVal_setVal(DoubleVal val, object &o, int from, int to)
{
PyObject  *op = o.ptr(); // no need for incrementing the refcnt while 'o' is alive
IndexRange rng(from, to);

	// if the object is not a python sequence then 'len()' cannot be used.
	// Thus, we have to handle this case separately...

	if ( ! PySequence_Check( op ) ) {
		// a single string (attempt to set enum) is also a sequence
		GILUnlocker allowThreadingWhileWaiting;
		return val->setVal( extract<double>( o ), &rng );
	}

	unsigned nelms = len(o);

	std::vector<double> v64;
	for ( unsigned i = 0; i < nelms; ++i ) {
		v64.push_back( extract<double>( o[i] ) );
	}
	{
	GILUnlocker allowThreadingWhileWaiting;
	return val->setVal( &v64[0], nelms, &rng );
	}
}


// wrap IPathVisitor to call back into python (assuming the visitor
// is implemented there, of course)
//
// Since shared_ptr<WrapPathVisitor> is a base class (of the wrapped class)
// the python refcound of 'self' is automatically taken care of :-)
//
// There is no need to acquire the GIL since we didn't release it in
// the first place (IPath::explore)
class WrapPathVisitor : public IPathVisitor {
private:
	PyObject *self_;

public:
	WrapPathVisitor(PyObject *self)
	: self_(self)
	{
	}

	virtual bool visitPre(ConstPath here)
	{
		return call_method<bool>(self_, "visitPre", here);
	}

	virtual void visitPost(ConstPath here)
	{
		call_method<void>(self_, "visitPost", here);
	}

	virtual ~WrapPathVisitor()
	{
	}
};

class WrapYamlFixup : public IYamlFixup, wrapper<IYamlFixup> {
private:
PyObject *self_;

public:

	WrapYamlFixup(PyObject *self)
	: self_(self)
	{
	}

	virtual void operator()(YAML::Node &root, YAML::Node &top)
	{
		call_method<void>(self_, "__call__", root, top);
	}

	virtual ~WrapYamlFixup()
	{
	}

};

static boost::python::tuple
wrap_Hub_getChildren(shared_ptr<IHub> h)
{
Hub hc(h);

	Children chldrn = hc->getChildren();

	Children::element_type::const_iterator it ( chldrn->begin() );
	Children::element_type::const_iterator ite( chldrn->end()   );

	// I would have preferred to convert the vector 'directly'
	// into a tuple - but 'make_tuple' kept producing compiler errors...

	boost::python::list l;

	while ( it != ite ) {
		l.append( *it );
		++it;
	}

	return boost::python::tuple( l );
}

// without the overload macros (using 'arg' within 'def') there
// are problems when a default pointer is set to 0.
// Complaints about mismatching python and c++ arg types (when using defaults)
BOOST_PYTHON_FUNCTION_OVERLOADS( IPath_loadYamlFile_ol,
                                 IPath::loadYamlFile,
                                 1, 4 )

BOOST_PYTHON_FUNCTION_OVERLOADS( wrap_Path_loadYamlStream_ol,
                                 wrap_Path_loadYamlStream,
                                 1, 4 )

BOOST_PYTHON_FUNCTION_OVERLOADS( wrap_Path_loadConfigFromYamlFile_ol,
                                 wrap_Path_loadConfigFromYamlFile,
                                 2, 3 )

BOOST_PYTHON_FUNCTION_OVERLOADS( wrap_Path_loadConfigFromYamlString_ol,
                                 wrap_Path_loadConfigFromYamlString,
                                 2, 3 )

BOOST_PYTHON_FUNCTION_OVERLOADS( wrap_Path_dumpConfigToYamlFile_ol,
                                 wrap_Path_dumpConfigToYamlFile,
                                 2, 4 )
BOOST_PYTHON_FUNCTION_OVERLOADS( wrap_Path_dumpConfigToYamlString_ol,
                                 wrap_Path_dumpConfigToYamlString,
                                 1, 3 )
BOOST_PYTHON_FUNCTION_OVERLOADS( setCPSWVerbosity_ol,
                                 setCPSWVerbosity,
                                 0, 2 )


BOOST_PYTHON_MODULE(pycpsw)
{
	PyEval_InitThreads();

	register_ptr_to_python<Child                          >();
	register_ptr_to_python<Children                       >();
	register_ptr_to_python<Hub                            >();
	register_ptr_to_python<Path                           >();
	register_ptr_to_python<ConstPath                      >();
	register_ptr_to_python<Enum                           >();
	register_ptr_to_python< shared_ptr<IVal_Base>         >();
	register_ptr_to_python< shared_ptr<IScalVal_Base>     >();
	register_ptr_to_python<ScalVal_RO                     >();
	register_ptr_to_python<ScalVal                        >();
	register_ptr_to_python<DoubleVal_RO                   >();
	register_ptr_to_python<DoubleVal                      >();
	register_ptr_to_python<Command                        >();
	register_ptr_to_python<Stream                         >();
	register_ptr_to_python<StreamMgr                      >();

	register_ptr_to_python< shared_ptr<std::string const> >();

	def("getCPSWVersionString", getCPSWVersionString);
	def("setCPSWVerbosity"    , setCPSWVerbosity    ,
		setCPSWVerbosity_ol(
			args("facility", "level"),
            "\n"
			"Set verbosity level for debugging messages of different\n"
			"Facilities. Invocation w/o arguments prints a list of\n"
			"currently supported facilities.\n"
		)
	);

	// wrap 'IEntry' interface
	class_<IEntry, boost::noncopyable>
	EntryClazz(
		"Entry",
		"\n"
		"Basic Node in the hierarchy",
		no_init
	);

	EntryClazz
		.def("getName",        &IEntry::getName,
			( arg("self") ),
			"\n"
			"Return the name of this Entry"
		)
		.def("getSize",        &IEntry::getSize,
			( arg("self") ),
			"\n"
			"Return the size (in bytes) of the entity represented by this Entry\n"
			"Note: if an array of Entries is present then 'getSize()' returns\n"
			"      the size of each element. In case of a compound/container element\n"
			"      the size of the entire compound is returned (including  arrays *in*\n"
			"      the container -- but not covering arrays of the container itself)."
		)
		.def("getDescription", &IEntry::getDescription,
			( arg("self") ),
			"\n"
			"Return the description string (if any) of this Entry."
		)
		.def("getPollSecs",    &IEntry::getPollSecs,
			( arg("self") ),
			"\n"
			"Return the suggested polling interval for this Entry."
		)
		.def("isHub",          &IEntry::isHub,
			( arg("self") ),
			"\n"
			"Test if this Entry is a Hub and return a reference.\n"
			"\n"
			"If this Entry is *not* a Hub then 'None' is returned."
		)
	;

	// wrap 'IChild' interface
	class_<IChild, bases<IEntry>, boost::noncopyable>
	ChildClazz(
		"Child",
		"\n"
		"An Entry which is attached to a Hub.\n"
		"\n",
		no_init);

	ChildClazz
		.def("getOwner",       &IChild::getOwner,
			( arg("self") ),
			"\n"
			"Return the Hub to which this Child is attached."
		)
		.def("getNelms",       &IChild::getNelms,
			( arg("self") ),
			"\n"
			"Return the number of elements this Child represents.\n"
			"\n"
			"For array nodes the return value is > 1.\n"
		)
	;


	// wrap 'IHub' interface
	class_<IHub, bases<IEntry>, boost::noncopyable>
	HubClazz("Hub",
		"\n"
		"Base class of all containers.",
		no_init
	);

	HubClazz
		.def("findByName",     &IHub::findByName,
			( arg("self"), arg("pathString") ),
			"\n"
			"find all entries matching 'path' in or underneath this hub.\n"
			"No wildcards are supported ATM. 'matching' refers to array\n"
			"indices which may be used in the path.\n"
			"\n"
			"E.g., if there is an array of four devices [0-3] then the\n"
			"path may select just two of them:\n"
			"\n"
			"     devices[1-2]\n"
			"\n"
			"omitting explicit indices selects *all* instances of an array."
			"\n"
			"SEE ALSO: Path.findByName.\n"
			"\n"
			"WARNING: do not use this method unless you know what you are\n"
			"         doing. A Path which does not originate at the root\n"
			"         cannot be used for communication and using it for\n"
			"         instantiating interfaces (ScalVal, Command, ...) will\n"
			"         result in I/O failures!\n"
		)
		.def("getChild",       &IHub::getChild,
			( arg("self"), arg("nameString") ),
			"\n"
			"Return a child with name 'nameString' (or 'None' if no matching child exists)."
		)
		.def("getChildren",    wrap_Hub_getChildren,
			( arg("self") ),
			"\n"
			"Return a list of all children"
		)
	;

	// wrap 'IPath' interface
	class_<IPath, boost::noncopyable>
	PathClazz(
		"Path",
		"\n"
		"Path objects are a 'handle' to a particular node in the hierarchy\n"
		"they encode complete routing information from the 'tail' node up\n"
		"to the root of the hierarchy. They also include index ranges for\n"
		"array members that are in the path.\n"
		"\n"
		"Paths are constructed by lookup and are passed to the factories\n"
		"that instantiate accessor interfaces such as 'ScalVal', 'Command'\n"
		"or 'Stream'.",
		no_init
	);

	PathClazz
		.def("findByName",   &IPath::findByName,
			( arg("self"), arg( "pathString" ) ),
			"\n"
			"looup 'pathString' starting from 'this' path and return a new Path object\n"
			"\n"
			"'pathString' (a string object) may span multiple levels in the hierarchy,\n"
			"separated by '/' - analogous to a file system.\n"
			"However, CPSW supports the concept of 'array nodes', i.e., some nodes\n"
			"(at any level of depth) may represent multiple identical devices or\n"
			"registers. Array indices are zero-based.\n"
			"\n"
			"When looking up an entity, the range(s) of items to address may be\n"
			"restricted by specifying index ranges. E.g., if there are 4 identical\n"
			"devices with ringbuffers of 32 elements each then a Path addressing\n"
			"all of these could be specified:\n"
			"\n"
			"      /prefix/device[0-3]/ringbuf[0-31]\n"
			"\n"
			"If the user wishes only to access ringbuffer locations 8 to 12 in the\n"
			"second device then they could construct a Path:\n"
			"\n"
			"      /prefix/device[1]/ringbuf[8-12]\n"
			"\n"
			"If a ScalVal is instantiated from this path then getVal/setVal would\n"
			"only read/write the desired subset of registers\n"
			"\n"
			"It should be noted that absence of a index specification is synonymous\n"
			"to covering the full range. In the above example:\n"
			"\n"
			"      /prefix/device/ringbuf\n"
			"\n"
			"would be identical to\n"
			"\n"
			"      /prefix/device[1]/ringbuf[8-12]\n"
			"\n"
			"A 'NotFoundError' is thrown if the target of the operation does not exist."
		)
		.def("__add__",      &IPath::findByName,
			( arg("self"), arg( "pathString" ) ),
			"\n"
			"Shortcut for 'findByName'"
		)
		.def("up",           &IPath::up,
			( arg("self") ),
			"\n"
			"Strip the last element off this path and return the child which\n"
			"was at the tail prior to the operation"
		)
		.def("empty",        &IPath::empty,
			( arg("self") ),
			"\n"
			"Test if this Path is empty returning 'True'/'False'"
		)
		.def(YAML_KEY_size,  &IPath::size,
			( arg("self") ),
			"\n"
			"Return the depth of this Path, i.e., how many '/' separated\n"
			"'levels' there are."
		)
		.def("clear",        wrap_Path_clear,
			( arg("self"), arg("hub") ),
			"\n"
			"Clear this Path and reset starting at 'hub'.\n"
			"\n"
			"Note: if 'hub' is not a true root device then future\n"
			"communication attempts may fail due to lack of routing\n"
			"information."
		)
		.def("origin",       &IPath::origin,
			( arg("self") ),
			"\n"
			"Return the Hub at the root of this path (if any -- 'None' otherwise)"
		)
		.def("parent",       &IPath::parent,
			( arg("self") ),
			"\n"
			"Return the parent Hub (if any -- 'None' otherwise)"
		)
		.def("tail",         &IPath::tail,
			( arg("self") ),
			"\n"
			"Return the child at the end of this Path (if any -- 'None' otherwise)"
		)
		.def("toString",     &IPath::toString,
			( arg("self") ),
			"\n"
			"Convert this Path to a string representation"
		)
		.def("__repr__",     &IPath::toString,
			( arg("self") ),
			"\n"
			"Convert this Path to a string representation"
		)
		.def("verifyAtTail", &IPath::verifyAtTail,
			( arg("self"), arg("path") ),
			"\n"
			"Verify that 'path' starts where 'this' Path ends; returns 'True'/'False'\n"
			"\n"
			"If this condition does not hold then the two Paths cannot be concatenated\n"
			"(and an exception would result)."
		)
		.def("append",       &IPath::append,
			( arg("self"), arg("path") ),
			"\n"
			"Append a copy of 'path' to 'this' one\n"
			"\n"
			"If verifyAtTail(path) returns 'False' then 'append' raises an 'InvalidPathError'."
		)
		.def("concat",       &IPath::concat,
			( arg("self"), arg("path") ),
			"\n"
			"Append a copy of 'path' to 'this' one and return a copy of the result.\n"
			"\n"
			"If verifyAtTail(path) returns 'False' then 'append' raises an 'InvalidPathError'.\n"
			"Note: 'append()' modifies the original path whereas 'concat' returns\n"
			"      a new copy."
		)
		.def("intersect",    &IPath::intersect,
			( arg("self"), arg("path") ),
			"\n"
			"Return a new Path which covers the intersection of this path and 'path'\n"
			"\n"
			"The new path contains all array elements common to both paths. Returns\n"
			"an empty path if there are no common elements or if the depth of the\n"
			"paths differs"
		)
		.def("isIntersecting",&IPath::isIntersecting,
			( arg("self"), arg("path") ),
			"\n"
			"A slightly more efficient version of 'intersect'\n"
			"\n"
			"If you only want to know if the intersection is (not)empty then isIntersecting\n"
			"is more efficent"
		)
		.def("clone",        &IPath::clone,
			( arg("self") ),
			"\n"
			"Return a copy of this Path."
		)
		.def("getNelms",     &IPath::getNelms,
			( arg("self") ),
			"\n"
			"Count and return the number of array elements addressed by this path.\n"
			"\n"
			"This sums up array elements at *all* levels. E.g., executed on a Path\n"
			"which represents 'device[0-3]/reg[0-3]' 'getNelms()' would yield 16."
		)
		.def("getTailFrom",  &IPath::getTailFrom,
			( arg("self") ),
			"\n"
			"Return the 'from' index addressed by the tail of this path.\n"
			"\n"
			"E.g., executed on a Path which represents 'device[0-3]/reg[1-2]'\n"
			"the method would yield 1. (The tail element is 'reg' and the 'from'\n"
			"index is 1.)"
		)
		.def("getTailTo",    &IPath::getTailTo,
			( arg("self") ),
			"\n"
			"Return the 'to' index addressed by the tail of this path.\n"
			"\n"
			"E.g., executed on a Path which represents 'device[0-3]/reg[1-2]'\n"
			"the method would yield 2. (The tail element is 'reg' and the 'to'\n"
			"index is 2.)"
		)
		.def("explore",      &IPath::explore,
			( arg("self"), arg("pathVisitor") ),
			"\n"
			"Recurse through the hierarchy rooted at 'this' Path and apply 'pathVisitor' to every descendent.\n"
			"\n"
			"See 'PathVisitor' for more information."
		)
		.def("loadConfigFromYamlFile", wrap_Path_loadConfigFromYamlFile,
			wrap_Path_loadConfigFromYamlFile_ol(
			args("self", "configYamlFilename", "yamlIncDirname"),
			"\n"
			"Load a configuration file in YAML format and write out into the hardware.\n"
			"\n"
			"'yamlIncDirname' may point to a directory where included YAML files can\n"
			"be found. Defaults to the directory where the YAML file is located.\n\n"
			"RETURNS: number of values successfully written out."
			)
		)
		.def("loadConfigFromYamlString", wrap_Path_loadConfigFromYamlString,
			wrap_Path_loadConfigFromYamlString_ol(
			args("self", "configYamlString", "yamlIncDirname"),
			"\n"
			"Load a configuration from a YAML formatted string and write out into the hardware.\n"
			"\n"
			"'yamlIncDirname' may point to a directory where included YAML files can be found.\n"
			"Defaults to the directory where the YAML file is located.\n\n"
			"RETURNS: number of values successfully written out."
			)
		)
		.def("dumpConfigToYamlFile", wrap_Path_dumpConfigToYamlFile,
			wrap_Path_dumpConfigToYamlFile_ol(
			args( "self", "fileName", "templFileName", "yamlIncDirname" ),
			"\n"
			"Read a configuration from hardware and save to a file in YAML format.\n"
			"If 'templFileName' is given then the paths listed in there (and only those)\n"
			"are used in the order defined in 'templFileName'. Otherwise, the 'configPrio'\n"
			"property of each field in the hierarchy is honored (see README.configData).\n"
			"'yamlIncDirname' can be used to specify where additional YAML files are\n"
			"stored.\n\n"
			"RETURNS: number of values successfully saved to file."
			)
		)
		.def("dumpConfigToYamlString", wrap_Path_dumpConfigToYamlString,
			wrap_Path_dumpConfigToYamlString_ol(
			args( "self", "templFileName", "yamlIncDirname" ),
			"\n"
			"Read a configuration from hardware and return as a string in YAML format."
			"If 'templFileName' is given then the paths listed in there (and only those)\n"
			"are used in the order defined in 'templFileName'. Otherwise, the 'configPrio'\n"
			"property of each field in the hierarchy is honored (see README.configData).\n"
			"'yamlIncDirname' can be used to specify where additional YAML files are\n"
			"stored."
			)
		)
		.def("create",       wrap_Path_create,
			( arg("hub") ),
			"\n"
			"Create a new Path originating at 'hub'\n"
			"\n"
			"Note: if 'hub' is not a true root device then future\n"
			"communication attempts may fail due to lack of routing\n"
			"information."
		)
		.staticmethod("create")
		.def("loadYamlFile",   &IPath::loadYamlFile,
			IPath_loadYamlFile_ol(
			args("yamlFileName", "rootName", "yamlIncDirName", "YamlFixup"),
			"\n"
			"Load a hierarchy definition in YAML format from a file.\n"
			"\n"
			"The hierarchy is built from the node with name 'rootName' (defaults to 'root').\n"
			"\n"
			"Optionally, 'yamlIncDirName' may be passed which identifies a directory\n"
			"where *all* yaml files reside. 'None' (or empty) instructs the method to\n"
			"use the same directory where 'fileName' resides.\n"
			"\n"
			"The directory is relevant for included YAML files.\n"
			"\n"
			"RETURNS: Root Path of the device hierarchy."
			)
		)
		.staticmethod("loadYamlFile")
		.def("loadYaml",       wrap_Path_loadYamlStream,
			wrap_Path_loadYamlStream_ol(
			args("yamlString", "rootName", "yamlIncDirName", "YamlFixup"),
			"\n"
			"Load a hierarchy definition in YAML format from a string.\n"
			"\n"
			"The hierarchy is built from the node with name 'rootName' (defaults to 'root').\n"
			"\n"
			"Optionally, 'yamlIncDirName' may be passed which identifies a directory\n"
			"where *all* yaml files reside. 'None' (or empty) instructs the method to\n"
			"use the current working directory.\n"
			"\n"
			"The directory is relevant for included YAML files.\n"
			"\n"
			"RETURNS: Root Path of the device hierarchy."
			)
		)
		.staticmethod("loadYaml")
	;

	class_<IPathVisitor, WrapPathVisitor, boost::noncopyable, shared_ptr<WrapPathVisitor> >
	WrapPathVisitorClazz(
		"PathVisitor",
		"\n"
		"Traverse the hierarchy.\n"
		"\n"
		"\n"
		"The user must implement an implementation for this\n"
		"interface which performs any desired action on each\n"
		"node in the hierarchy.\n"
		"\n"
		"The 'IPath::explore()' method accepts an IPathVisitor\n"
		"and recurses through the hierarchy calling into\n"
		"IPathVisitor from each visited node.\n"
		"\n"
		"The user must implement 'visitPre()' and 'visitPost()'\n"
		"methods. Both methods are invoked for each node in the\n"
		"hierarchy and a Path object leading to the node is supplied\n"
		"to these methods. The user typically includes state information\n"
		"in his/her implementation of the interface.\n"
		"\n"
		"visitPre() is executed before recursing into children of 'path'.\n"
		"If the method returns 'False' then no recursion into children\n"
		"occurs.\n"
		"\n"
		"   bool visitPre(Path path)\n"
		"\n"
		"visiPost() is invoked after recursion into children and even\n"
		"if recursion was skipped due to 'visitPre()' returning 'False'\n"
		"\n"
		"   void visitPost(Path path)\n"
		"\n"
	);

	class_<IYamlFixup, WrapYamlFixup, boost::noncopyable, shared_ptr<WrapYamlFixup> >
	WrapYamlFixupClazz(
		"YamlFixup",
		"\n"
		"The user must implement an implementation for this\n"
		"interface which performs any desired fixup on the YAML\n"
		"root node which is passed to the 'fixup' method\n"
		"\n"
		"NOTE: you need python bindings for the yaml-cpp library\n"
		"      in order to use this. Such bindings are NOT part\n"
		"      of CPSW!\n"
        "\n"
		"      void operator()(YAML::Node &root, YAML::Node &top)\n"
		"\n"
		"      In python you must implement __call__(self, root, top)\n"
	);

	WrapYamlFixupClazz
	.def("__call__", pure_virtual( &IYamlFixup::operator() ))
	.def(
		"findByName", &IYamlFixup::findByName,
		( arg("node"), arg("path"), arg("sep") = '/' ),
		"\n"
		"Lookup a YAML node from 'node' traversing a hierarchy\n"
		"of YAML::Map's while handling merge keys ('<<').\n"
		"The path to lookup is a string with path elements\n"
		"separated by 'sep'\n"
	)
	.staticmethod("findByName")
	;

	// wrap 'IEnum' interface
	class_<IEnum, boost::noncopyable>
	Enum_Clazz(
		"Enum",
		"\n"
		"An Enum object is a list of (string, numerical value) tuples.",
		no_init
	);

	Enum_Clazz
		.def("getNelms",     &IEnum::getNelms,
			( arg("self") ),
			"\n"
			"Return the number of entries in this Enum."
		)
		.def("getItems",     wrap_Enum_getItems,
			( arg("self") ),
			"\n"
			"Return this enum converted into a python list.\n"
			"We chose this over a dictionary (to which the list\n"
			"can easily be converted) because it preserves the\n"
			"original order of the menu entreis."
		)
	;

	// wrap 'IVal_Base' interface
	class_<IVal_Base, bases<IEntry>, boost::noncopyable>
	Val_BaseClazz(
		"Val_Base",
		"\n"
		"Base class for Val variants.\n",
		no_init
	);

	Val_BaseClazz
		.def("getNelms",     &IVal_Base::getNelms,
			( arg("self") ),
			"\n"
			"Return number of elements addressed by this ScalVal.\n"
			"\n"
			"The Path used to instantiate a ScalVal may address an array\n"
			"of scalar values. This method returns the number of array elements"
		)
		.def("getPath",      &IVal_Base::getPath,
			( arg("self") ),
			"\n"
			"Return a copy of the Path which was used to create this ScalVal."
		)
		.def("getEncoding", wrap_Val_Base_getEncoding,
			( arg("self") ),
			"\n"
			"Return a string which describes the encoding if this ScalVal is\n"
			"itself e.g., a string. Common encodings include 'ASCII', 'UTF_8\n"
			"Custom encodings are also communicated as 'CUSTOM_<integer>'.\n"
			"If this Val has no defined encoding then the encoding should be <None>\n"
			"(i.e., the object None).\n\n"
			"The encoding may e.g., be used to convert a numerical array into\n"
			"a string by python (second argument to the str() constructor).\n\n"
			"The encoding 'IEEE_754' conveys that the ScalVal represents a\n"
			"floating-point number.\n\n"
		)
		.def("__repr__",     wrap_Val_Base_repr,
			( arg("self") ),
			"\n"
			"Convert this ScalVal to a string representation\n"
			"showing the Path that identifies it."
		)
		.def("create",       &IVal_Base::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'Val_Base' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod(			"create")
	;

	// Asynchronous IO interface
	class_<IPyAsyncIO, CAsyncGetValWrapperContext, boost::noncopyable, AsyncGetValWrapperContext > ("AsyncIO")
	;

	// wrap 'IScalVal_Base' interface
	class_<IScalVal_Base, bases<IVal_Base>, boost::noncopyable>
	ScalVal_BaseClazz(
		"ScalVal_Base",
		"\n"
		"Base class for ScalVal variants.\n",
		no_init
	);

	ScalVal_BaseClazz
		.def("getSizeBits",  &IScalVal_Base::getSizeBits,
			( arg("self") ),
			"\n"
			"Return the size in bits of this ScalVal.\n"
			"\n"
			"If the ScalVal represents an array then the return value is the size\n"
			"of each individual element."
		)
		.def(YAML_KEY_isSigned,     &IScalVal_Base::isSigned,
			( arg("self") ),
			"\n"
			"Return True if this ScalVal represents a signed number.\n"
			"\n"
			"If the ScalVal is read into a wider number than its native bitSize\n"
			"then automatic sign-extension is performed (for signed ScalVals)."
		)
		.def("getEnum",      &IScalVal_Base::getEnum,
			( arg("self") ),
			"\n"
			"Return 'Enum' object associated with this ScalVal (if any).\n"
			"\n"
			"An Enum object is a list of (str,int) tuples with associates strings\n",
			"with numerical values."
		)
		.def("create",       &IScalVal_Base::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'ScalVal_Base' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod(			"create")
	;

	// wrap 'IScalVal_RO' interface
	class_<IScalVal_RO, bases<IScalVal_Base>, boost::noncopyable>
	ScalVal_ROClazz(
		"ScalVal_RO",
		"\n"
		"Read-Only interface for endpoints which support scalar values.\n"
		"\n"
		"This interface supports reading integer values e.g., registers\n"
		"or individual bits. It may also feature an associated map of\n"
		"'enum strings'. E.g., a bit with such a map attached could be\n"
		"read as 'True' or 'False'.\n"
		"\n"
		"NOTE: If no write operations are required then it is preferable\n"
		"      to use the ScalVal_RO interface (as opposed to ScalVal)\n"
		"      since the underlying endpoint may be read-only.",
		no_init
	);

	ScalVal_ROClazz
		.def("getVal",       wrap_ScalVal_RO_getVal,
			( arg("self"), arg("fromIdx") = -1, arg("toIdx") = -1, arg("forceNumeric") = false ),
			"\n"
			"Read one or multiple values, return as a scalar or a list.\n"
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the ScalVal_RO was created are retrieved. All\n"
			"values are represented by a 'c-style flattened' list:\n"
			"\n"
			"  /some/dev[0-1]/item[0-9]\n"
			"\n"
			"would return\n"
			"\n"
			"  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].\n"
			"\n"
			"The indices may be used to 'slice' the rightmost index in the path which\n"
			"was used when creating the ScalVal interface. Enumerating these slices\n"
			"starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx\n"
			"**are based off the index contained in the path**!\n"
			"E.g., if a ScalVal_RO created from the above path is read with:\n"
			"\n"
			"  ScalVal_RO.create( root.findByName('/some/dev[0-1]/item[4-9]') )->getVal( 1, 2 )\n"
			"\n"
			"then [ dev0_item5, dev0_item6, dev1_item5, dev1_item6 ] would be returned.\n"
			"Note how 'fromIdx==1' is added to the starting index '4' from the path.\n"
			"If both 'fromIdx' and 'toIdx' are negative then all elements are included.\n"
			"A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only\n"
			"the single element at 'fromIdx' to be read.\n"
			"\n"
			"If the ScalVal_RO supports an 'enum' menu then the numerical values read from the\n"
			"underlying hardware are mapped back to strings which are returned by 'getVal()'.\n"
			"The optional 'forceNumeric' argument, when set to 'True', suppresses this\n"
			"conversion and fetches the raw numerical values."
		)
		.def("getValAsync",             wrap_ScalVal_RO_getValAsync,
			( arg("self"), arg("AsyncIO"), arg("fromIdx") = -1, arg("toIdx") = -1, arg("forceNumeric") = false ),
			"Provide an asynchronous callback which will be executed once data arrive"
		)
		.def(			"getVal",       wrap_ScalVal_RO_getVal_into,
			( arg(			"self"), arg("bufObject"), arg("fromIdx") = -1, arg("toIdx") = -1),
			"\n"
			"Read one or multiple values into a buffer, return the number of items read.\n"
			"\n"
			"This variant of 'getVal()' reads values directly into 'bufObject' which must\n"
			"support the ('new-style') buffer protocol (e.g., numpy.ndarray).\n"
			"If the buffer is larger than the number of elements retrieved (i.e., the\n"
			"return value of this method) then the excess elements are undefined.\n"
			"\n"
			"The index limits ('fromIdx/toIdx') may be used to restrict the number of\n"
			"elements read as described above.\n"
			"\n"
			"No mapping to enum strings is supported by this variant of 'getVal()'."
		)
		.def(			"create",       &IScalVal_RO::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'ScalVal_RO' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod(			"create")
	;

	// wrap 'IScalVal' interface
	class_<IScalVal, bases<IScalVal_RO>, boost::noncopyable>
	ScalVal_Clazz(
		"ScalVal",
		"\n"
		"Interface for endpoints which support scalar values.\n"
		"\n"
		"This interface supports reading/writing integer values e.g., registers\n"
		"or individual bits. It may also feature an associated map of\n"
		"'enum strings'. E.g., a bit with such a map attached could be\n"
		"read/written as 'True' or 'False'.",
		no_init
	);

	ScalVal_Clazz
		.def(			"setVal",       wrap_ScalVal_setVal,
		    ( arg(			"self"), arg("values"), arg("fromIdx") = -1, arg("toIdx") = -1 ),
			"\n"
			"Write one or multiple values, return the number of elements written."
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the ScalVal was created are written. All values\n"
			"represented by a 'c-style flattened' list or array:\n"
			"\n"
			"  /some/dev[0-1]/item[0-9]\n"
			"\n"
			"would expect\n"
			"\n"
			"  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].\n"
			"\n"
			"The indices may be used to 'slice' the rightmost index in the path which\n"
			"was used when creating the ScalVal interface. Enumerating these slices\n"
			"starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx\n"
			"**are based off the index contained in the path**!\n"
			"E.g., if a ScalVal created from the above path is written with:\n"
			"\n"
			"  ScalVal.create( root.findByName('/some/dev[0-1]/item[4-9]') )->setVal( [44,55], 1, 1 )\n"
			"\n"
			"then dev[0]/item[5] would be written with 44 and dev[1]/item[5] with 55.\n"
			"Note how 'fromIdx==1' is added to the starting index '4' from the path.\n"
			"If both 'fromIdx' and 'toIdx' are negative then all elements are included.\n"
			"A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only\n"
			"the single element at 'fromIdx' to be written.\n"
			"\n"
			"If the ScalVal has an associated enum 'menu' and 'values' are strings then\n"
			"these strings are mapped by the enum to raw numerical values.\n"
			"\n"
			"If the 'values' object implements the (new-style) buffer protocol then 'setVal()'\n"
			"will extract values directly from the buffer. No enum strings are supported in\n"
			"this case."
		)
		.def("create",       &IScalVal::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'ScalVal' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;

	// wrap 'IDoubleVal_RO' interface
	class_<IDoubleVal_RO, bases<IVal_Base>, boost::noncopyable>
	DoubleVal_ROClazz(
		"DoubleVal_RO",
		"\n"
		"Read-Only interface for endpoints which support 'double' values.\n"
		"\n"
		"This interface is analogous to ScalVal_RO but reads from an\n"
		"interface which supplies floating-point numbers.\n"
		"\n"
		"NOTE: If no write operations are required then it is preferable\n"
		"      to use the DoubleVal_RO interface (as opposed to DoubleVal)\n"
		"      since the underlying endpoint may be read-only.",
		no_init
	);

	DoubleVal_ROClazz
		.def("getVal",       wrap_DoubleVal_RO_getVal,
			( arg("self"), arg("fromIdx") = -1, arg("toIdx") = -1 ),
			"\n"
			"Read one or multiple values, return as a scalar or a list.\n"
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the DoubleVal_RO was created are retrieved. All\n"
			"values are represented by a 'c-style flattened' list:\n"
			"\n"
			"  /some/dev[0-1]/item[0-9]\n"
			"\n"
			"would return\n"
			"\n"
			"  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].\n"
			"\n"
			"The indices may be used to 'slice' the rightmost index in the path which\n"
			"was used when creating the DoubleVal interface. Enumerating these slices\n"
			"starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx\n"
			"**are based off the index contained in the path**!\n"
			"E.g., if a DoubleVal_RO created from the above path is read with:\n"
			"\n"
			"  DoubleVal_RO.create( root.findByName('/some/dev[0-1]/item[4-9]') )->getVal( 1, 2 )\n"
			"\n"
			"then [ dev0_item5, dev0_item6, dev1_item5, dev1_item6 ] would be returned.\n"
			"Note how 'fromIdx==1' is added to the starting index '4' from the path.\n"
			"If both 'fromIdx' and 'toIdx' are negative then all elements are included.\n"
			"A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only\n"
			"the single element at 'fromIdx' to be read.\n"
		)
		.def("getValAsync",             wrap_DoubleVal_RO_getValAsync,
			( arg("self"), arg("AsyncIO"), arg("fromIdx") = -1, arg("toIdx") = -1 ),
			"Provide an asynchronous callback which will be executed once data arrive"
		)
		.def(			"create",       &IDoubleVal_RO::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'DoubleVal_RO' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod(			"create")
	;

	// wrap 'IDoubleVal' interface
	class_<IDoubleVal, bases<IDoubleVal_RO>, boost::noncopyable>
	DoubleVal_Clazz(
		"DoubleVal",
		"\n"
		"Interface for endpoints which support 'double' values.\n"
		"\n"
		"This interface is analogous to ScalVal but accesses an\n"
		"interface which expects/supplies floating-point numbers.",
		no_init
	);

	DoubleVal_Clazz
		.def(			"setVal",       wrap_DoubleVal_setVal,
		    ( arg(			"self"), arg("values"), arg("fromIdx") = -1, arg("toIdx") = -1 ),
			"\n"
			"Write one or multiple values, return the number of elements written."
			"\n"
			"If no indices (fromIdx, toIdx) are specified then all elements addressed by\n"
			"the path object from which the DoubleVal was created are written. All values\n"
			"represented by a 'c-style flattened' list or array:\n"
			"\n"
			"  /some/dev[0-1]/item[0-9]\n"
			"\n"
			"would expect\n"
			"\n"
			"  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].\n"
			"\n"
			"The indices may be used to 'slice' the rightmost index in the path which\n"
			"was used when creating the DoubleVal interface. Enumerating these slices\n"
			"starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx\n"
			"**are based off the index contained in the path**!\n"
			"E.g., if a DoubleVal created from the above path is written with:\n"
			"\n"
			"  DoubleVal.create( root.findByName('/some/dev[0-1]/item[4-9]') )->setVal( [44,55], 1, 1 )\n"
			"\n"
			"then dev[0]/item[5] would be written with 44 and dev[1]/item[5] with 55.\n"
			"Note how 'fromIdx==1' is added to the starting index '4' from the path.\n"
			"If both 'fromIdx' and 'toIdx' are negative then all elements are included.\n"
			"A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only\n"
			"the single element at 'fromIdx' to be written.\n"
		)
		.def("create",       &IDoubleVal::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'DoubleVal' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;


	// wrap 'IStream' interface
	class_<IStream, bases<IVal_Base>, boost::noncopyable>
	Stream_Clazz(
		"Stream",
		"\n"
		"Interface for endpoints with support streaming of raw data.\n"
		"NOTE: A stream must be explicitly managed (i.e., destroyed/closed\n"
		"      when nobody reads from it in order to avoid RSSI stalls).\n"
        "      In python you must use a StreamMgr as a context manager\n"
		"      and use the stream within a 'with' statement."
		,
		no_init
	);

	Stream_Clazz
		.def("read",         wrap_Stream_read,
			( arg("self"), arg("bufObject"), arg("timeoutUs") = -1, arg("offset") = 0 ),
			"\n"
			"Read raw bytes from a streaming interface into a buffer and return the number of bytes read.\n"
			"\n"
			"'bufObject' must support the (new-style) buffer protocol.\n"
			"\n"
			"The 'timeoutUs' argument may be used to limit the time this\n"
			"method blocks waiting for data to arrive. A (relative) timeout\n"
			"in micro-seconds may be specified. A negative timeout blocks\n"
			"indefinitely."
		)
		.def("write",        wrap_Stream_write,
			( arg("self"), arg("bufObject"), arg("timeoutUs") = 0 ),
			"\n"
			"Write raw bytes to a streaming interface from a buffer and return the number of bytes written.\n"
			"\n"
			"'bufObject' must support the (new-style) buffer protocol.\n"
			"\n"
			"The 'timeoutUs' argument may be used to limit the time this\n"
			"method blocks waiting for data to be accepted. A (relative) timeout\n"
			"in micro-seconds may be specified. A negative timeout blocks\n"
			"indefinitely."
		)
		.def("create",    &CStreamMgr::create,
			( arg("path") ),
			"\n"
			"Instantiate a ***'StreamMgr'*** context manager. Note that the Stream itself\n"
			"is created/deleted by the manager's __enter__/__exit__."
			"\n"
		)
		.staticmethod("create")
	;

	class_<CStreamMgr, bases<IStream>, boost::noncopyable>
	StreamMgr_Clazz(
		"StreamMgr",
		"\n"
		"Context Manager for CPSW Stream",
		no_init
	);

	StreamMgr_Clazz
		.def("__enter__", &CStreamMgr::__enter__,
			( arg("self") ),
            "\n"
            "Entering the context instantiates the actual Stream; it may now be used.\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.def("__exit__",  &CStreamMgr::__exit__,
			( arg("self"), arg("exc_type"), arg("exc_value"), arg("traceback") ),
			"\n"
			"Leave the context and destroy the Stream associated with the context."
		)
		.def("create",    &CStreamMgr::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'StreamMgr' context manager. Note that the Stream itself\n"
			"is created/deleted by the manager's __enter__/__exit__."
			"\n"
		)
		.staticmethod("create")
	;

	// wrap 'ICommand' interface
	class_<ICommand, bases<IEntry>, boost::noncopyable>
	Command_Clazz(
		"Command",
		"\n"
		"The Command interface gives access to commands implemented by the underlying endpoint.\n"
		"\n"
		"Details of the command are hidden. Execution runs the command or command sequence\n"
		"coded by the endpoint.",
		no_init
	);

	Command_Clazz
		.def("execute",      wrap_Command_execute,
			"\n"
			"Execute the command implemented by the endpoint addressed by the\n"
			"path which was created when instantiating the Command interface."
		)
		.def("getPath",      &ICommand::getPath,
			( arg("self") ),
			"\n"
			"Return a copy of the Path which was used to create this Command."
		)
		.def("create",       &ICommand::create,
			( arg("path") ),
			"\n"
			"Instantiate a 'Stream' interface at the endpoint identified by 'path'\n"
			"\n"
			"NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does\n"
			"      not support this interface."
		)
		.staticmethod("create")
	;

	// these macros must all be executed from the same scope so
	// that the 'translator' objects of base classes are still
	// 'alive'.

	ExceptionTranslatorInstall(CPSWError);
	ExceptionTranslatorInstallDerived(DuplicateNameError,           CPSWError);
	ExceptionTranslatorInstallDerived(NotDevError,                  CPSWError);
	ExceptionTranslatorInstallDerived(NotFoundError,                CPSWError);
	ExceptionTranslatorInstallDerived(InvalidPathError,             CPSWError);
	ExceptionTranslatorInstallDerived(InvalidIdentError,            CPSWError);
	ExceptionTranslatorInstallDerived(InvalidArgError,              CPSWError);
	ExceptionTranslatorInstallDerived(AddressAlreadyAttachedError,  CPSWError);
	ExceptionTranslatorInstallDerived(ConfigurationError,           CPSWError);
	ExceptionTranslatorInstallDerived(ErrnoError,                   CPSWError);
	ExceptionTranslatorInstallDerived(InternalError,                ErrnoError);
	ExceptionTranslatorInstallDerived(AddrOutOfRangeError,          CPSWError);
	ExceptionTranslatorInstallDerived(ConversionError,              CPSWError);
	ExceptionTranslatorInstallDerived(InterfaceNotImplementedError, CPSWError);
	ExceptionTranslatorInstallDerived(IOError,                      ErrnoError);
	ExceptionTranslatorInstallDerived(BadStatusError,               CPSWError);
	ExceptionTranslatorInstallDerived(IntrError,                    CPSWError);
	ExceptionTranslatorInstallDerived(StreamDoneError,              CPSWError);
	ExceptionTranslatorInstallDerived(FailedStreamError,            CPSWError);
	ExceptionTranslatorInstallDerived(MissingOnceTagError,          CPSWError);
	ExceptionTranslatorInstallDerived(MissingIncludeFileNameError,  CPSWError);
	ExceptionTranslatorInstallDerived(NoYAMLSupportError,           CPSWError);
	ExceptionTranslatorInstallDerived(NoError,                      CPSWError);
	ExceptionTranslatorInstallDerived(MultipleInstantiationError,   CPSWError);
	ExceptionTranslatorInstallDerived(BadSchemaVersionError,        CPSWError);
	ExceptionTranslatorInstallDerived(TimeoutError,                 CPSWError);

}

}
