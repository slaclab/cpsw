 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

%module(directors = 1) pycpsw
%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_pair.i>
%include <std_vector.i>

%ignore "";

%shared_ptr(IYamlSupportBase)
%shared_ptr(IEntry)
%shared_ptr(IChild)
%shared_ptr(IPath)
%shared_ptr(IHub)
%shared_ptr(IEnum)
%shared_ptr(IVal_Base)
%shared_ptr(IScalVal_Base)
%shared_ptr(IScalVal_RO)
%shared_ptr(IScalVal_WO)
%shared_ptr(IScalVal)
%shared_ptr(ICommand)
%shared_ptr(IDoubleVal_RO)
%shared_ptr(IDoubleVal_WO)
%shared_ptr(IDoubleVal)
%shared_ptr(IStream)
%shared_ptr(CAsyncIOWrapper)

%{
    #include "cpsw_api_user.h"
    #include "cpsw_swig_python.h"
    #include "cpsw_python.h"
    #include <yaml-cpp/yaml.h>
    using namespace cpsw_python;
%}

%init %{
    PyEval_InitThreads();
%}

/* If I don't explicitly ignore these operators then I see warnings... */
%ignore IEnum::IIterator;
%ignore IEnum::IIterator::operator++;
%ignore IEnum::iterator;
%ignore IEnum::iterator::operator++;
%ignore IEnum::iterator::operator=;
%ignore IndexRange::operator++;

%rename("%s")                   getCPSWVersionString;
%rename("%s")                   setCPSWVerbosity;

%rename("Entry")                IEntry;
%rename("%s")                   IEntry::~IEntry;
%rename("%s")                   IEntry::getName;
%rename("%s")                   IEntry::getSize;
%rename("%s")                   IEntry::getDescription;
%rename("%s")                   IEntry::getPollSecs;
%rename("%s")                   IEntry::isHub;

%rename("Child")                IChild;
%rename("%s")                   IChild::~IChild;
%rename("%s")                   IChild::getOwner;
%rename("%s")                   IChild::getNelms;

%rename("Hub")                  IHub;
%rename("%s")                   IHub::~IHub;
%rename("%s")                   IHub::getChild;
%rename("%s")                   IHub::getChildren;

%rename("Path")                 IPath;
%rename("%s")                   IPath::~IPath;
%rename("%s")                   IPath::findByName;
%rename("%s")                   IPath::up;
%rename("%s")                   IPath::empty;
%rename("%s")                   IPath::size;
%rename("%s")                   IPath::clear;
%rename("%s")                   IPath::create;
%rename("%s")                   IPath::origin;
%rename("%s")                   IPath::parent;
%rename("%s")                   IPath::tail;
%rename("%s")                   IPath::toString;
%rename("%s")                   IPath::verifyAtTail;
%rename("%s")                   IPath::append;
%rename("%s")                   IPath::concat;
%rename("%s")                   IPath::intersect;
%rename("%s")                   IPath::isIntersecting;
%rename("%s")                   IPath::clone;
%rename("%s")                   IPath::getNelms;
%rename("%s")                   IPath::getTailFrom;
%rename("%s")                   IPath::getTailTo;
%rename("%s")                   IPath::explore;
%rename("%s")                   IPath::loadYamlFile;
%rename("%s")                   IPath::loadConfigFromYamlFile;
%rename("%s")                   IPath::loadConfigFromYamlString;
%rename("%s")                   IPath::dumpConfigToYamlFile;
%rename("%s")                   IPath::dumpConfigToYamlString;
%ignore                         IPath::loadYamlStream(std::istream&, char const* root_name = "root", char const * yaml_dir_name=0, IYamlFixup* fixup = 0);
%ignore                         IPath::loadYamlStream(char const * , char const* root_name = "root", char const * yaml_dir_name=0, IYamlFixup* fixup = 0);
%rename("loadYaml")             IPath::loadYamlStream(const std::string &yaml, const char *root_name = "root", const char *yaml_dir_name = 0, IYamlFixup *fixup = 0);
%extend IPath {
    uint64_t
    loadConfigFromYamlFile(const char *yamlFile, const char *yaml_dir=0);

    uint64_t
    loadConfigFromYamlString(const char *yaml,  const char *yaml_dir = 0);

    uint64_t
    dumpConfigToYamlFile(const char *filename, const char *templFilename = 0, const char *yaml_dir = 0);

    std::string
    dumpConfigToYamlString(const char *templFilename = 0, const char *yaml_dir = 0);

    static Path
    loadYamlStream(const std::string &yaml, const char *root_name = "root", const char *yaml_dir_name = 0, IYamlFixup *fixup = 0)
    {
        return wrap_Path_loadYamlStream(yaml, root_name, yaml_dir_name, fixup);
    }

    Path
    __add__(const char *name)
    {
        return $self->findByName( name );
    }
}

%feature("python:slot", "tp_repr", functype="reprfunc") IPath::toString;

%rename("Val_Base")             IVal_Base;
%rename("%s")                   IVal_Base::~IVal_Base;
%rename("%s")                   IVal_Base::getNelms;
%rename("%s")                   IVal_Base::getPath;
//FIXME
%rename("%s")                   IVal_Base::getEncoding;
%rename("%s")                   IVal_Base::create;
// dummy method so we get a wrapper which we can use
// for the __repr__ slot
%rename("%s")                   IVal_Base::repr;
%extend IVal_Base {
    std::string
    repr();
};

%rename("Enum")                 IEnum;
%rename("%s")                   IEnum::~IEnum;
%rename("%s")                   IEnum::getNelms;
%rename("%s")                   IEnum::getItems;
%extend IEnum {
    PyObject *getItems()
    {
    IEnum::iterator it = $self->begin();
    IEnum::iterator ie = $self->end();
    PyUniqueObj     lis( PyList_New( 0 ) );
        while ( it != ie ) {

            PyUniqueObj tup( PyTuple_New( 2 ) );
            PyUniqueObj idx( PyLong_FromUnsignedLongLong( (*it).second ) );
            PyUniqueObj nam( PyUnicode_FromString( (*it).first->c_str()) );

            if ( PyTuple_SetItem( tup.get(), 0, nam.get() ) ) {
                PyErr_SetString(PyExc_RuntimeError, "typemap(out) Enum: error; unable to set tuple item 'name'");
                return NULL;
            }
            nam.release();

            if ( PyTuple_SetItem( tup.get(), 1, idx.get() ) ) {
                PyErr_SetString(PyExc_RuntimeError, "typemap(out) Enum: error; unable to set tuple item 'index'");
                return NULL;
            }
            idx.release();

            if ( PyList_Append( lis.get(), tup.get() ) ) {
                PyErr_SetString(PyExc_RuntimeError, "typemap(out) Enum: error; unable to append to list");
                return NULL;
            }
            tup.release();
            ++it;
        }
        return lis.release();
    }
};

%rename("AsyncIO")              CAsyncIOWrapper;
%rename("%s")                   CAsyncIOWrapper::~CAsyncIOWrapper;
%rename("callback")             CAsyncIOWrapper::py_callback;

%rename("ScalVal_Base")         IScalVal_Base;
%rename("%s")                   IScalVal_Base::~IScalVal_Base;
%rename("%s")                   IScalVal_Base::getSizeBits;
%rename("%s")                   IScalVal_Base::isSigned;
%rename("%s")                   IScalVal_Base::getEnum;
%rename("%s")                   IScalVal_Base::create;

%rename("ScalVal_RO")           IScalVal_RO;
%rename("%s")                   IScalVal_RO::~IScalVal_RO;
%rename("%s")                   IScalVal_RO::create;
%rename("%s")                   IScalVal_RO::getVal(int fromIdx = -1, int toIdx = -1,bool forceNumeric = false);
%rename("%s")                   IScalVal_RO::getVal(PyObject *buf,int fromIdx = -1,int toIdx = -1);
%rename("%s")                   IScalVal_RO::getValAsync(std::shared_ptr<CAsyncIOWrapper>);
%extend                         IScalVal_RO {
    PyObject *
    getVal(int fromIdx = -1, int toIdx = -1, bool forceNumeric = false);

    unsigned
    getVal(PyObject *buf, int fromIdx = -1, int toIdx = -1);

    unsigned
    getValAsync(std::shared_ptr<CAsyncIOWrapper> aio)
    {
        // FIXME
        throw InternalError("IScalVal_RO::getValAsync not ported to SWIG yet");
        return 0;
    }
    
};

%ignore                         IScalVal_WO;

%rename("ScalVal")              IScalVal;
%rename("%s")                   IScalVal::~IScalVal;
%rename("%s")                   IScalVal::setVal(PyObject *values,int fromIdx = -1, int toIdx = -1);
%extend                         IScalVal {
    unsigned
    setVal(PyObject *values, int fromIdx = -1, int toIdx = -1);
};

%rename("DoubleVal_RO")         IDoubleVal_RO;
%rename("%s")                   IDoubleVal_RO::~IDoubleVal_RO;
%rename("%s")                   IDoubleVal_RO::getVal(int fromIdx = -1, int toIdx = -1);
%rename("%s")                   IDoubleVal_RO::getValAsync(std::shared_ptr<CAsyncIOWrapper>);
%extend                         IDoubleVal_RO {
    PyObject *
    getVal(int fromIdx = -1, int toIdx = -1);

    unsigned
    getValAsync(std::shared_ptr<CAsyncIOWrapper> aio)
    {  
        // FIXME
        throw InternalError("IDoubleVal_RO::getValAsync not ported to SWIG yet");
        return 0;
    }
};

%ignore                         IDoubleVal_WO;

%rename("DoubleVal")            IDoubleVal;
%rename("%s")                   IDoubleVal::~IDoubleVal;
//%rename("%s")                   IDoubleVal::setVal(PyObject *values,int fromIdx = -1, int toIdx = -1);
//%extend                         IDoubleVal {
//    unsigned
//    setVal(PyObject *values, int fromIdx = -1, int toIdx = -1);
//};


%rename("Command")              ICommand;
%rename("%s")                   ICommand::~ICommand;
%rename("%s")                   ICommand::getPath;
%rename("%s")                   ICommand::create;
%extend ICommand {
    void
    execute();
};

%rename("PathVisitor")          IPathVisitor;
%rename("%s")                   IPathVisitor::~IPathVisitor;
%rename("%s")                   IPathVisitor::visitPre;
%rename("%s")                   IPathVisitor::visitPost;

%rename("YamlFixup")            IYamlFixup;
%rename("%s")                   IYamlFixup::~IYamlFixup;
// operator() seems to be handled/recognized anyways by the director.
// If I leave the 'rename' in then I get a warning
//%rename("%s")                   IYamlFixup::operator();
%rename("%s")                   IYamlFixup::findByName;

%ignore                         cpsw_python::handleException;
%rename("_registerExceptions_") cpswSwigRegisterExceptions;

%pythoncode %{
from sys import modules as sys_modules
_pycpsw._registerExceptions_( sys_modules[__name__] )
del sys_modules
%}

%template(PairIntInt)    std::pair<int,int>;
%template(VecPairIntInt) std::vector< std::pair<int, int> >;

%feature("python:tp_repr") IVal_Base "_wrap_Val_Base_repr";

%typemap(out)               Children
{
Children children = result;
Children::element_type::const_iterator it( children->begin() );
Children::element_type::const_iterator ie( children->end()   );
Py_ssize_t i;

    PyUniqueObj tup( PyTuple_New( children->size() ) );
    i = 0;
    while ( it != ie ) {
        std::shared_ptr< const IChild > *smartresult = new std::shared_ptr< const IChild >( *it );
        PyObject *o = SWIG_NewPointerObj(%as_voidptr(smartresult), $descriptor(std::shared_ptr<IChild>*), SWIG_POINTER_OWN);

        if ( PyTuple_SetItem( tup.get(), i, o ) ) {
            Py_DECREF( o );
            PyErr_SetString(PyExc_RuntimeError, "typemap(out) Children: error; unable to set tuple item");
            SWIG_fail;
        }
        ++it;
        ++i;
    }
    $result = tup.release();
}

%exception {
    try {
        $action
    } catch (const CPSWError &e) {
        cpsw_python::handleException();
        SWIG_fail;
    }
}

%feature("director") IYamlFixup;
%feature("director") IPathVisitor;
%feature("director") CAsyncIOWrapper;

/* Swig currently (V3) does not handle a 'using std::shared_ptr' clause correctly */
#define shared_ptr std::shared_ptr

%include "cpsw_api_user.h"
%include "cpsw_swig_python.h"
