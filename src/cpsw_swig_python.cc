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

static PyObject *pCpswPyExc_InternalError                = 0;
static PyObject *pCpswPyExc_IOError                      = 0;
static PyObject *pCpswPyExc_ErrnoError                   = 0;
static PyObject *pCpswPyExc_DuplicateNameError           = 0;
static PyObject *pCpswPyExc_NotDevError                  = 0;
static PyObject *pCpswPyExc_NotFoundError                = 0;
static PyObject *pCpswPyExc_InvalidPathError             = 0;
static PyObject *pCpswPyExc_InvalidIdentError            = 0;
static PyObject *pCpswPyExc_InvalidArgError              = 0;
static PyObject *pCpswPyExc_AddressAlreadyAttachedError  = 0;
static PyObject *pCpswPyExc_ConfigurationError           = 0;
static PyObject *pCpswPyExc_AddrOutOfRangeError          = 0;
static PyObject *pCpswPyExc_ConversionError              = 0;
static PyObject *pCpswPyExc_InterfaceNotImplementedError = 0;
static PyObject *pCpswPyExc_BadStatusError               = 0;
static PyObject *pCpswPyExc_IntrError                    = 0;
static PyObject *pCpswPyExc_StreamDoneError              = 0;
static PyObject *pCpswPyExc_FailedStreamError            = 0;
static PyObject *pCpswPyExc_MissingOnceTagError          = 0;
static PyObject *pCpswPyExc_MissingIncludeFileNameError  = 0;
static PyObject *pCpswPyExc_NoYAMLSupportError           = 0;
static PyObject *pCpswPyExc_NoError                      = 0;
static PyObject *pCpswPyExc_MultipleInstantiationError   = 0;
static PyObject *pCpswPyExc_BadSchemaVersionError        = 0;
static PyObject *pCpswPyExc_TimeoutError                 = 0;
static PyObject *pCpswPyExc_CPSWError                    = 0;

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

void
cpsw_python::handleException()
{
	try {
		throw;
	}
	catch ( InternalError &e ) {
		PyErr_SetString( pCpswPyExc_InternalError, e.what() );
	}
	catch ( IOError &e ) {
		PyErr_SetString( pCpswPyExc_IOError, e.what() );
	}
	catch ( ErrnoError &e ) {
		PyErr_SetString( pCpswPyExc_ErrnoError, e.what() );
	}
	catch ( DuplicateNameError &e ) {
		PyErr_SetString( pCpswPyExc_DuplicateNameError, e.what() );
	}
	catch ( NotDevError &e ) {
		PyErr_SetString( pCpswPyExc_NotDevError, e.what() );
	}
	catch ( NotFoundError &e ) {
		PyErr_SetString( pCpswPyExc_NotFoundError, e.what() );
	}
	catch ( InvalidPathError &e ) {
		PyErr_SetString( pCpswPyExc_InvalidPathError, e.what() );
	}
	catch ( InvalidIdentError &e ) {
		PyErr_SetString( pCpswPyExc_InvalidIdentError, e.what() );
	}
	catch ( InvalidArgError &e ) {
		PyErr_SetString( pCpswPyExc_InvalidArgError, e.what() );
	}
	catch ( AddressAlreadyAttachedError &e ) {
		PyErr_SetString( pCpswPyExc_AddressAlreadyAttachedError, e.what() );
	}
	catch ( ConfigurationError &e ) {
		PyErr_SetString( pCpswPyExc_ConfigurationError, e.what() );
	}
	catch ( AddrOutOfRangeError &e ) {
		PyErr_SetString( pCpswPyExc_AddrOutOfRangeError, e.what() );
	}
	catch ( ConversionError &e ) {
		PyErr_SetString( pCpswPyExc_ConversionError, e.what() );
	}
	catch ( InterfaceNotImplementedError &e ) {
		PyErr_SetString( pCpswPyExc_InterfaceNotImplementedError, e.what() );
	}
	catch ( BadStatusError &e ) {
		PyErr_SetString( pCpswPyExc_BadStatusError, e.what() );
	}
	catch ( IntrError &e ) {
		PyErr_SetString( pCpswPyExc_IntrError, e.what() );
	}
	catch ( StreamDoneError &e ) {
		PyErr_SetString( pCpswPyExc_StreamDoneError, e.what() );
	}
	catch ( FailedStreamError &e ) {
		PyErr_SetString( pCpswPyExc_FailedStreamError, e.what() );
	}
	catch ( MissingOnceTagError &e ) {
		PyErr_SetString( pCpswPyExc_MissingOnceTagError, e.what() );
	}
	catch ( MissingIncludeFileNameError &e ) {
		PyErr_SetString( pCpswPyExc_MissingIncludeFileNameError, e.what() );
	}
	catch ( NoYAMLSupportError &e ) {
		PyErr_SetString( pCpswPyExc_NoYAMLSupportError, e.what() );
	}
	catch ( NoError &e ) {
		PyErr_SetString( pCpswPyExc_NoError, e.what() );
	}
	catch ( MultipleInstantiationError &e ) {
		PyErr_SetString( pCpswPyExc_MultipleInstantiationError, e.what() );
	}
	catch ( BadSchemaVersionError &e ) {
		PyErr_SetString( pCpswPyExc_BadSchemaVersionError, e.what() );
	}
	catch ( TimeoutError &e ) {
		PyErr_SetString( pCpswPyExc_TimeoutError, e.what() );
	}
	catch ( CPSWError &e ) {
		PyErr_SetString( pCpswPyExc_CPSWError, e.what() );
	}
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
