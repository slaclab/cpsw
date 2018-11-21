%module pycpsw
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

/* FIXME */
%ignore                         IYamlFixup;
%ignore                         IPathVisitor;
    
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
    
%ignore                         cpsw_python::handleException;
%rename("_registerExceptions_") cpsw_python::registerExceptions;

%{
    #include "cpsw_api_user.h"
    #include "cpsw_swig_python.h"
%}

%pythoncode %{
from sys import modules as sys_modules
_pycpsw._registerExceptions_( sys_modules[__name__] )
del sys_modules
%}

%template(PairIntInt)    std::pair<int,int>;
%template(VecPairIntInt) std::vector< std::pair<int, int> >;

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

/* Swig currently (V3) does not handle a 'using std::shared_ptr' clause correctly */
#define shared_ptr std::shared_ptr

%include "cpsw_error.h"
%include "cpsw_api_user.h"
%include "cpsw_swig_python.h"
