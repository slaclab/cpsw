 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <cpsw_python.h>
#include <cpsw_cython.h>
#include <stdexcept>

extern "C" {

extern PyTypeObject CpswPyExcT_InternalError;
PyObject * pCpswPyExc_InternalError = (PyObject *)&CpswPyExcT_InternalError;
extern PyTypeObject CpswPyExcT_IOError;
PyObject * pCpswPyExc_IOError = (PyObject *)&CpswPyExcT_IOError;
extern PyTypeObject CpswPyExcT_ErrnoError;
PyObject * pCpswPyExc_ErrnoError = (PyObject *)&CpswPyExcT_ErrnoError;
extern PyTypeObject CpswPyExcT_DuplicateNameError;
PyObject * pCpswPyExc_DuplicateNameError = (PyObject *)&CpswPyExcT_DuplicateNameError;
extern PyTypeObject CpswPyExcT_NotDevError;
PyObject * pCpswPyExc_NotDevError = (PyObject *)&CpswPyExcT_NotDevError;
extern PyTypeObject CpswPyExcT_NotFoundError;
PyObject * pCpswPyExc_NotFoundError = (PyObject *)&CpswPyExcT_NotFoundError;
extern PyTypeObject CpswPyExcT_InvalidPathError;
PyObject * pCpswPyExc_InvalidPathError = (PyObject *)&CpswPyExcT_InvalidPathError;
extern PyTypeObject CpswPyExcT_InvalidIdentError;
PyObject * pCpswPyExc_InvalidIdentError = (PyObject *)&CpswPyExcT_InvalidIdentError;
extern PyTypeObject CpswPyExcT_InvalidArgError;
PyObject * pCpswPyExc_InvalidArgError = (PyObject *)&CpswPyExcT_InvalidArgError;
extern PyTypeObject CpswPyExcT_AddressAlreadyAttachedError;
PyObject * pCpswPyExc_AddressAlreadyAttachedError = (PyObject *)&CpswPyExcT_AddressAlreadyAttachedError;
extern PyTypeObject CpswPyExcT_ConfigurationError;
PyObject * pCpswPyExc_ConfigurationError = (PyObject *)&CpswPyExcT_ConfigurationError;
extern PyTypeObject CpswPyExcT_AddrOutOfRangeError;
PyObject * pCpswPyExc_AddrOutOfRangeError = (PyObject *)&CpswPyExcT_AddrOutOfRangeError;
extern PyTypeObject CpswPyExcT_ConversionError;
PyObject * pCpswPyExc_ConversionError = (PyObject *)&CpswPyExcT_ConversionError;
extern PyTypeObject CpswPyExcT_InterfaceNotImplementedError;
PyObject * pCpswPyExc_InterfaceNotImplementedError = (PyObject *)&CpswPyExcT_InterfaceNotImplementedError;
extern PyTypeObject CpswPyExcT_BadStatusError;
PyObject * pCpswPyExc_BadStatusError = (PyObject *)&CpswPyExcT_BadStatusError;
extern PyTypeObject CpswPyExcT_IntrError;
PyObject * pCpswPyExc_IntrError = (PyObject *)&CpswPyExcT_IntrError;
extern PyTypeObject CpswPyExcT_StreamDoneError;
PyObject * pCpswPyExc_StreamDoneError = (PyObject *)&CpswPyExcT_StreamDoneError;
extern PyTypeObject CpswPyExcT_FailedStreamError;
PyObject * pCpswPyExc_FailedStreamError = (PyObject *)&CpswPyExcT_FailedStreamError;
extern PyTypeObject CpswPyExcT_MissingOnceTagError;
PyObject * pCpswPyExc_MissingOnceTagError = (PyObject *)&CpswPyExcT_MissingOnceTagError;
extern PyTypeObject CpswPyExcT_MissingIncludeFileNameError;
PyObject * pCpswPyExc_MissingIncludeFileNameError = (PyObject *)&CpswPyExcT_MissingIncludeFileNameError;
extern PyTypeObject CpswPyExcT_NoYAMLSupportError;
PyObject * pCpswPyExc_NoYAMLSupportError = (PyObject *)&CpswPyExcT_NoYAMLSupportError;
extern PyTypeObject CpswPyExcT_NoError;
PyObject * pCpswPyExc_NoError = (PyObject *)&CpswPyExcT_NoError;
extern PyTypeObject CpswPyExcT_MultipleInstantiationError;
PyObject * pCpswPyExc_MultipleInstantiationError = (PyObject *)&CpswPyExcT_MultipleInstantiationError;
extern PyTypeObject CpswPyExcT_BadSchemaVersionError;
PyObject * pCpswPyExc_BadSchemaVersionError = (PyObject *)&CpswPyExcT_BadSchemaVersionError;
extern PyTypeObject CpswPyExcT_TimeoutError;
PyObject * pCpswPyExc_TimeoutError = (PyObject *)&CpswPyExcT_TimeoutError;
extern PyTypeObject CpswPyExcT_CPSWError;
PyObject * pCpswPyExc_CPSWError = (PyObject *)&CpswPyExcT_CPSWError;

};


namespace cpsw_python {

bool
CPathVisitor::visitPre(ConstPath here)
{
	return visitPre(me(), here);
}

void
CPathVisitor::visitPost(ConstPath here)
{
	visitPost(me(), here);
}

void
CYamlFixup::operator()(YAML::Node &root, YAML::Node &top)
{
	call( me(), root, top );
}

CAsyncIO::CAsyncIO(PyObject *pyObj)
: CPyBase( pyObj )
{
}

CAsyncIO::CAsyncIO()
{
}


void
CAsyncIO::callback(CPSWError *err)
{
GILGuard guard;
{

PyUniqueObj result;
PyUniqueObj status;

	if ( err ) {
		/* Build argument list */
		PyUniqueObj arg ( PyUnicode_FromString( err->what() ) );
		PyUniqueObj args( PyTuple_New(1) );
		if ( ! args || ! arg || PyTuple_SetItem( args.get(), 0, arg.get() ) ) {
			throw std::runtime_error("CAsyncIO::callback -- unable to create Python object for arguments");
		}
		arg.release();
		/* call constructor; note that PyObject_New() just creates an object
		 * and maybe sets its type but does not execute the constructor.
		 */
		PyUniqueObj tmp( PyObject_Call( translateException(err), args.get(), NULL ) );
		status = cpsw::move( tmp );
	} else {
		complete(0).transfer( result );
	}

	/* Call into Python */
	callback( me(), result.release(), status.release() );

}
}

void
CAsyncIO::init(PyObject *pObj)
{
	if ( me_ && pObj != me_ )
		throw InternalError("CAsyncIO::init() may only be called once!");
	me_ = pObj;
}

class CAsyncIODeletor {
public:
	void operator()(CAsyncIO *aio)
	{
	/* called from CPSW thread; must acquire GIL first */
	GILGuard guard;
	{
		Py_DECREF( aio->me() );
	}
	}
};

shared_ptr<CAsyncIO>
CAsyncIO::makeShared()
{
/* This is called from cython (i.e., with GIL held) */
	Py_INCREF( me() );
	return shared_ptr<CAsyncIO>( this, CAsyncIODeletor() );
}

CAsyncIO::~CAsyncIO()
{
}

};
