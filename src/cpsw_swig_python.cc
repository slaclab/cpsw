 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_swig_python.h>
#include <cpsw_python.h>
#include <string>

using namespace cpsw_python;

PyObject *pCpswPyExc_InternalError                = 0;
PyObject *pCpswPyExc_IOError                      = 0;
PyObject *pCpswPyExc_ErrnoError                   = 0;
PyObject *pCpswPyExc_DuplicateNameError           = 0;
PyObject *pCpswPyExc_NotDevError                  = 0;
PyObject *pCpswPyExc_NotFoundError                = 0;
PyObject *pCpswPyExc_InvalidPathError             = 0;
PyObject *pCpswPyExc_InvalidIdentError            = 0;
PyObject *pCpswPyExc_InvalidArgError              = 0;
PyObject *pCpswPyExc_AddressAlreadyAttachedError  = 0;
PyObject *pCpswPyExc_ConfigurationError           = 0;
PyObject *pCpswPyExc_AddrOutOfRangeError          = 0;
PyObject *pCpswPyExc_ConversionError              = 0;
PyObject *pCpswPyExc_InterfaceNotImplementedError = 0;
PyObject *pCpswPyExc_BadStatusError               = 0;
PyObject *pCpswPyExc_IntrError                    = 0;
PyObject *pCpswPyExc_StreamDoneError              = 0;
PyObject *pCpswPyExc_FailedStreamError            = 0;
PyObject *pCpswPyExc_MissingOnceTagError          = 0;
PyObject *pCpswPyExc_MissingIncludeFileNameError  = 0;
PyObject *pCpswPyExc_NoYAMLSupportError           = 0;
PyObject *pCpswPyExc_NoError                      = 0;
PyObject *pCpswPyExc_MultipleInstantiationError   = 0;
PyObject *pCpswPyExc_BadSchemaVersionError        = 0;
PyObject *pCpswPyExc_TimeoutError                 = 0;
PyObject *pCpswPyExc_CPSWError                    = 0;

static PyObject *makeException(PyObject *m, const char *name, PyObject *base)
{
std::string qual = std::string(PyModule_GetName( m )) + "." + std::string(name);

PyObject *exc = PyErr_NewException( qual.c_str(), base, NULL );
	Py_INCREF( exc ); // type objects live forever
	PyModule_AddObject( m, name, exc );
	return exc;
}

#define ExceptionTranslatorInstall(mod, clazz) \
	do { pCpswPyExc_##clazz = makeException(mod, #clazz, 0); } while ( 0)

#define ExceptionTranslatorInstallDerived(mod, clazz, bazz) \
	do { pCpswPyExc_##clazz = makeException(mod, #clazz, pCpswPyExc_##bazz); } while ( 0)

void
cpswSwigRegisterExceptions(PyObject *module)
{
	ExceptionTranslatorInstallDerived(module, InternalError,                ErrnoError);
	ExceptionTranslatorInstallDerived(module, IOError,                      ErrnoError);
	ExceptionTranslatorInstallDerived(module, ErrnoError,                   CPSWError);
	ExceptionTranslatorInstallDerived(module, DuplicateNameError,           CPSWError);
	ExceptionTranslatorInstallDerived(module, NotDevError,                  CPSWError);
	ExceptionTranslatorInstallDerived(module, NotFoundError,                CPSWError);
	ExceptionTranslatorInstallDerived(module, InvalidPathError,             CPSWError);
	ExceptionTranslatorInstallDerived(module, InvalidIdentError,            CPSWError);
	ExceptionTranslatorInstallDerived(module, InvalidArgError,              CPSWError);
	ExceptionTranslatorInstallDerived(module, AddressAlreadyAttachedError,  CPSWError);
	ExceptionTranslatorInstallDerived(module, ConfigurationError,           CPSWError);
	ExceptionTranslatorInstallDerived(module, AddrOutOfRangeError,          CPSWError);
	ExceptionTranslatorInstallDerived(module, ConversionError,              CPSWError);
	ExceptionTranslatorInstallDerived(module, InterfaceNotImplementedError, CPSWError);
	ExceptionTranslatorInstallDerived(module, BadStatusError,               CPSWError);
	ExceptionTranslatorInstallDerived(module, IntrError,                    CPSWError);
	ExceptionTranslatorInstallDerived(module, StreamDoneError,              CPSWError);
	ExceptionTranslatorInstallDerived(module, FailedStreamError,            CPSWError);
	ExceptionTranslatorInstallDerived(module, MissingOnceTagError,          CPSWError);
	ExceptionTranslatorInstallDerived(module, MissingIncludeFileNameError,  CPSWError);
	ExceptionTranslatorInstallDerived(module, NoYAMLSupportError,           CPSWError);
	ExceptionTranslatorInstallDerived(module, NoError,                      CPSWError);
	ExceptionTranslatorInstallDerived(module, MultipleInstantiationError,   CPSWError);
	ExceptionTranslatorInstallDerived(module, BadSchemaVersionError,        CPSWError);
	ExceptionTranslatorInstallDerived(module, TimeoutError,                 CPSWError);
	ExceptionTranslatorInstall(module, CPSWError);
}

typedef CGetValWrapperContextTmpl<PyUniqueObj, PyListObj> CGetValWrapperContext;

CAsyncIOWrapper::CAsyncIOWrapper()
: ctxt_( new CGetValWrapperContext() )
{
}

CAsyncIOWrapper::~CAsyncIOWrapper()
{
}

void
CAsyncIOWrapper::callback(CPSWError *err)
{
	PyGILState_STATE state_ = PyGILState_Ensure();

	/* Call into Python */

    PyUniqueObj result = ctxt_->complete( err );

	py_callback( result.release() );

	PyGILState_Release( state_ );
}
    
PyObject *
IScalVal_RO_getVal(IScalVal_RO *val, int fromIdx, int toIdx, bool forceNumeric)
{
	/* Need to hack around in order to get the shared pointer back... */
    CGetValWrapperContext ctxt;
        ctxt.issueGetVal( val, fromIdx, toIdx, forceNumeric, AsyncIO() );
	PyUniqueObj o = std::move( ctxt.complete( 0 ) );
	return o.release();
}
