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

%shared_ptr(IYamlSupportBase)
%shared_ptr(IEntry)
%shared_ptr(IChild)
%shared_ptr(IPath)
%shared_ptr(IHub)
%shared_ptr(IEnum)
%shared_ptr(IVal_Base)
%shared_ptr(IScalVal_Base)
%shared_ptr(IAsyncIO)
%shared_ptr(IScalVal_RO)
%shared_ptr(IScalVal_WO)
%shared_ptr(IScalVal)
%shared_ptr(ICommand)
%shared_ptr(IDoubleVal_RO)
%shared_ptr(IDoubleVal_WO)
%shared_ptr(IDoubleVal)
%shared_ptr(IStream)

%{
    #include "cpsw_api_user.h"
    #include "cpsw_swig_python.h"
    #include "cpsw_python.h"
    #include <yaml-cpp/yaml.h>
    using namespace cpsw_python;
%}

/* FIXME */
%ignore IEnum::IIterator;
%ignore IEnum::IIterator::operator++;
%ignore IEnum::iterator;
%ignore IEnum::iterator::operator++;
%ignore IEnum::iterator::operator=;
%ignore IndexRange::operator++;
/*
%shared_ptr(std::string)
%shared_ptr(IEventSource)
%shared_ptr(std::vector<Child>)
*/

    
%rename("Entry")                IEntry;
%rename("Child")                IChild;
%rename("Path")                 IPath;
%rename("Hub")                  IHub;
%rename("Enum")                 IEnum;
%rename("Val_Base")             IVal_Base;
%rename("ScalVal_Base")         IScalVal_Base;
%rename("AsyncIO")              IAsyncIO;
%rename("ScalVal_RO")           IScalVal_RO;
%ignore                         IScalVal_WO;
%rename("ScalVal")              IScalVal;
%rename("Command")              ICommand;
%rename("DoubleVal_RO")         IDoubleVal_RO;
%ignore                         IDoubleVal_WO;
%rename("DoubleVal")            IDoubleVal;
%rename("Stream")               IStream;
%rename("PathVisitor")          IPathVisitor;
%rename("YamlFixup")            IYamlFixup;
    
%ignore                         cpsw_python::handleException;
%rename("_registerExceptions_") cpsw_python::registerExceptions;

%ignore                         IPath::loadYamlStream(std::istream&, char const* root_name = "root", char const * yaml_dir_name=0, IYamlFixup* fixup = 0);
%ignore                         IPath::loadYamlStream(char const * , char const* root_name = "root", char const * yaml_dir_name=0, IYamlFixup* fixup = 0);

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
}

%extend IVal_Base {
    std::string
    repr();
};

%pythoncode %{
from sys import modules as sys_modules
_pycpsw._registerExceptions_( sys_modules[__name__] )
del sys_modules
%}

%template(PairIntInt)    std::pair<int,int>;
%template(VecPairIntInt) std::vector< std::pair<int, int> >;

%feature("python:tp_repr") IVal_Base "_wrap_Val_Base_repr";
%feature("python:slot", "tp_repr", functype="reprfunc") IPath::toString;

%typemap(out)               Children
{
Children children = result;
Children::element_type::const_iterator it( children->begin() );
Children::element_type::const_iterator ie( children->end()   );
Py_ssize_t i;

    PyObject *tup = PyTuple_New( children->size() );
    i = 0;
    while ( it != ie ) {
        std::shared_ptr< const IChild > *smartresult = new std::shared_ptr< const IChild >( *it );
        PyObject *o = SWIG_NewPointerObj(SWIG_as_voidptr(smartresult), SWIGTYPE_p_std__shared_ptrT_IChild_t, SWIG_POINTER_OWN);

        if ( PyTuple_SetItem( tup, i, o ) ) {
            Py_DECREF( o );
            PyErr_SetString(PyExc_RuntimeError, "typemap(out) Children: error; unable to set tuple item");
            SWIG_fail;
        }
        ++it;
        ++i;
    }
    $result = tup;
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

/* Swig currently (V3) does not handle a 'using std::shared_ptr' clause correctly */
#define shared_ptr std::shared_ptr

%include "cpsw_api_user.h"
%include "cpsw_swig_python.h"

