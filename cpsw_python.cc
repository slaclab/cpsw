#include <cpsw_api_user.h>
#include <boost/python.hpp>
#include <boost/python/tuple.hpp>
#include <boost/python/list.hpp>
#include <boost/python/dict.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <vector>

using namespace boost::python;

namespace cpsw_python {

// Translate CPSW Errors/exceptions into python exceptions

template <typename ECL>
class ExceptionTranslator {
private:
	PyObject *pyObj_;
public:

	ExceptionTranslator(const char *NAM)
	{
	class_<ECL> EClazz(NAM, no_init);
			EClazz.def("what", &CPSWError::what);
		pyObj_ = EClazz.ptr();
	}

	void operator() (const ECL &e) const
	{
		PyErr_SetObject(pyObj_, boost::python::object(e).ptr());
	}

#define ExceptionTranslatorInstall(clazz) register_exception_translator<clazz>( ExceptionTranslator<clazz>(#clazz) )
};

static void wrap_Path_loadConfigFromYaml(Path p, const char *filename)
{
YAML::Node conf( YAML::LoadFile( filename ) );
	p->loadConfigFromYaml( conf );
}

class ofs : public std::fstream {
public:
	ofs(const char *filename)
	: std::fstream( filename, out )
	{
	}

	~ofs()
	{
		close();
	}
};

static void wrap_Path_dumpConfigToYaml(Path p, const char *filename)
{
YAML::Node conf;
	p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

ofs strm( filename );
	strm << emit.c_str() << "\n";
}

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

static boost::python::dict wrap_Enum_getItems(Enum enm)
{
boost::python::dict d;

IEnum::iterator it  = enm->begin();
IEnum::iterator ite = enm->end();
	while ( it != ite ) {
		d[ *(*it).first ] = (*it).second;
		++it;
	}

	return d;
}

// FIXME: write a better function; one which also supports the buffer interface
static boost::python::object wrap_ScalVal_RO_getVal(ScalVal_RO val)
{
boost::python::object o;

Enum     enm   = val->getEnum();
unsigned nelms = val->getNelms();
unsigned got;

	if ( enm ) {

	std::vector<CString>  str;

		str.reserve(nelms);
		got = val->getVal( &str[0], nelms );
		if ( 1 == got ) {
			return boost::python::object( *str[0] );	
		}

		boost::python::list l;
		for ( unsigned i = 0; i<got; i++ ) {
			l.append( *str[i] );
		}
		return l;
	} else {

	std::vector<uint64_t> v64;

		got = val->getVal( &v64[0], nelms );
		if ( 1 == got ) {
			return boost::python::object( v64[0] );
		}

		boost::python::list l;
		for ( unsigned i = 0; i<got; i++ ) {
			l.append( v64[i] );
		}
		return l;
	}
}

static void wrap_ScalVal_setVal(ScalVal val, object &o)
{
}

// FIXME: write a better function; one which also supports the buffer interface
static boost::python::object wrap_ScalVal_RO_setVal(ScalVal_RO val)
{
}

// wrap IPathVisitor to call back into python (assuming the visitor
// is implemented there, of course)
class WrapPathVisitor : public IPathVisitor {
private:
	PyObject *self_;

public:
	WrapPathVisitor(PyObject *self)
	: self_(self)
	{
		std::cout << "creating WrapPathVisitor\n";
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
		std::cout << "deleting WrapPathVisitor\n";
	}
};

BOOST_PYTHON_MODULE(pycpsw)
{

	register_ptr_to_python<Child                          >();
	register_ptr_to_python<Hub                            >();
	register_ptr_to_python<Path                           >();
	register_ptr_to_python<ConstPath                      >();
	register_ptr_to_python<Enum                           >();
	register_ptr_to_python< shared_ptr<IScalVal_Base>     >();
	register_ptr_to_python<ScalVal_RO                     >();

	register_ptr_to_python< shared_ptr<std::string const> >();

	// wrap 'IEntry' interface
	class_<IEntry, boost::noncopyable> EntryClazz("Entry", no_init);
	EntryClazz
		.def("getName",        &IEntry::getName)
		.def("getSize",        &IEntry::getSize)
		.def("getDescription", &IEntry::getDescription)
		.def("isHub",          &IEntry::isHub)
	;

	// wrap 'IChild' interface
	class_<IChild, bases<IEntry>, boost::noncopyable> ChildClazz("Child", no_init);
	ChildClazz
		.def("getOwner",       &IChild::getOwner)
		.def("getNelms",       &IChild::getNelms)
	;

	// wrap 'IHub' interface
	class_<IHub, bases<IEntry>, boost::noncopyable> HubClazz("Hub", no_init);
	HubClazz
		.def("findByName",     &IHub::findByName)
		.def("getChild",       &IHub::getChild)
		.def("loadYamlFile",   &IHub::loadYamlFile)
		.staticmethod("loadYamlFile")
	;

	// wrap 'IPath' interface
    class_<IPath, boost::noncopyable> PathClazz("Path", no_init);
	PathClazz
		.def("findByName",   &IPath::findByName)
		.def("up",           &IPath::up)
		.def("empty",        &IPath::empty)
		.def("size",         &IPath::size)
		.def("clear",        wrap_Path_clear)
		.def("origin",       &IPath::origin)
		.def("parent",       &IPath::parent)
		.def("tail",         &IPath::tail)
		.def("toString",     &IPath::toString)
		.def("verifyAtTail", &IPath::verifyAtTail)
		.def("append",       &IPath::append)
		.def("explore",      &IPath::explore)
		.def("concat",       &IPath::concat)
		.def("clone",        &IPath::clone)
		.def("getNelms",     &IPath::getNelms)
		.def("getTailFrom",  &IPath::getTailFrom)
		.def("getTailTo",    &IPath::getTailTo)
		.def("loadConfigFromYaml", wrap_Path_loadConfigFromYaml)
		.def("dumpConfigToYaml",   wrap_Path_dumpConfigToYaml)
		.def("create",       wrap_Path_create)
		.staticmethod("create")
	;

	class_<IPathVisitor, WrapPathVisitor, boost::noncopyable, boost::shared_ptr<WrapPathVisitor> > WrapPathVisitorClazz("PathVisitor");

	// wrap 'IEnum' interface
	class_<IEnum, boost::noncopyable> Enum_Clazz("Enum", no_init);
	Enum_Clazz
		.def("getNelms",     &IEnum::getNelms)
		.def("getItems",     wrap_Enum_getItems)
	;

	// wrap 'IScalVal_Base' interface
	class_<IScalVal_Base, bases<IEntry>, boost::noncopyable> ScalVal_BaseClazz("ScalVal_Base", no_init);
	ScalVal_BaseClazz
		.def("getNelms",     &IScalVal_Base::getNelms)
		.def("getSizeBits",  &IScalVal_Base::getSizeBits)
		.def("isSigned",     &IScalVal_Base::isSigned)
		.def("getPath",      &IScalVal_Base::getPath)
		.def("getEnum",      &IScalVal_Base::getEnum)
	;

	// wrap 'IScalVal_RO' interface
	class_<IScalVal_RO, bases<IScalVal_Base>, boost::noncopyable> ScalVal_ROClazz("ScalVal_RO", no_init);
	ScalVal_ROClazz
		.def("getVal",       wrap_ScalVal_RO_getVal)
		.def("create",       &IScalVal_RO::create)
		.staticmethod("create")
	;

	// wrap 'IScalVal' interface
	class_<IScalVal, bases<IScalVal_RO>, boost::noncopyable> ScalVal_Clazz("ScalVal", no_init);
	ScalVal_Clazz
		.def("setVal",       wrap_ScalVal_setVal)
		.def("create",       &IScalVal::create)
		.staticmethod("create")
	;

	// wrap 'ICommand' interface
	class_<ICommand, bases<IEntry>, boost::noncopyable> Command_Clazz("Command", no_init);
	Command_Clazz
		.def("execute",      &ICommand::execute)
		.def("create",       &ICommand::create)
		.staticmethod("create")
	;

	ExceptionTranslatorInstall(CPSWError);
	ExceptionTranslatorInstall(DuplicateNameError);
	ExceptionTranslatorInstall(NotDevError);
	ExceptionTranslatorInstall(NotFoundError);
	ExceptionTranslatorInstall(InvalidPathError);
	ExceptionTranslatorInstall(InvalidIdentError);
	ExceptionTranslatorInstall(InvalidArgError);
	ExceptionTranslatorInstall(AddressAlreadyAttachedError);
	ExceptionTranslatorInstall(ConfigurationError);
	ExceptionTranslatorInstall(ErrnoError);
	ExceptionTranslatorInstall(InternalError);
	ExceptionTranslatorInstall(AddrOutOfRangeError);
	ExceptionTranslatorInstall(ConversionError);
	ExceptionTranslatorInstall(InterfaceNotImplementedError);
	ExceptionTranslatorInstall(IOError);
	ExceptionTranslatorInstall(BadStatusError);
	ExceptionTranslatorInstall(IntrError);
	ExceptionTranslatorInstall(StreamDoneError);
	ExceptionTranslatorInstall(FailedStreamError);
	ExceptionTranslatorInstall(MissingOnceTagError);
	ExceptionTranslatorInstall(MissingIncludeFileNameError);
	ExceptionTranslatorInstall(NoYAMLSupportError);

}

}
