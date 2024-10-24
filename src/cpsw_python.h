 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* Generic python helpers */

#ifndef CPSW_PYTHON_H
#define CPSW_PYTHON_H

#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <Python.h>
#include <string>
#include <vector>
#include <iostream>

#define PYCPSW_CYTHON 	1
#define PYCPSW_BOOST	2
#define PYCPSW_SWIG		3

extern "C" {
	extern PyObject *pCpswPyExc_InternalError;
	extern PyObject *pCpswPyExc_IOError;
	extern PyObject *pCpswPyExc_ErrnoError;
	extern PyObject *pCpswPyExc_DuplicateNameError;
	extern PyObject *pCpswPyExc_NotDevError;
	extern PyObject *pCpswPyExc_NotFoundError;
	extern PyObject *pCpswPyExc_InvalidPathError;
	extern PyObject *pCpswPyExc_InvalidIdentError;
	extern PyObject *pCpswPyExc_InvalidArgError;
	extern PyObject *pCpswPyExc_AddressAlreadyAttachedError;
	extern PyObject *pCpswPyExc_ConfigurationError;
	extern PyObject *pCpswPyExc_AddrOutOfRangeError;
	extern PyObject *pCpswPyExc_ConversionError;
	extern PyObject *pCpswPyExc_InterfaceNotImplementedError;
	extern PyObject *pCpswPyExc_BadStatusError;
	extern PyObject *pCpswPyExc_IntrError;
	extern PyObject *pCpswPyExc_StreamDoneError;
	extern PyObject *pCpswPyExc_FailedStreamError;
	extern PyObject *pCpswPyExc_MissingOnceTagError;
	extern PyObject *pCpswPyExc_MissingIncludeFileNameError;
	extern PyObject *pCpswPyExc_NoYAMLSupportError;
	extern PyObject *pCpswPyExc_NoError;
	extern PyObject *pCpswPyExc_MultipleInstantiationError;
	extern PyObject *pCpswPyExc_BadSchemaVersionError;
	extern PyObject *pCpswPyExc_TimeoutError;
	extern PyObject *pCpswPyExc_CPSWError;
};

namespace cpsw_python {

	// To be used from a 'catch' block
	void handleException();

    PyObject *translateException(CPSWError *err);

class GILUnlocker {
private:
	GILUnlocker(const GILUnlocker &);
	GILUnlocker &operator=(const GILUnlocker &);

	PyThreadState *_save;
public:
	GILUnlocker()
	{
		Py_UNBLOCK_THREADS;
	}

	~GILUnlocker()
	{
		Py_BLOCK_THREADS;
	}
};

class GILGuard {
private:
	GILGuard(const GILGuard &);
	GILGuard &operator=(const GILGuard &);
	PyGILState_STATE state_;
public:
	GILGuard()
	: state_( PyGILState_Ensure() )
	{
	}

	~GILGuard()
	{
		PyGILState_Release( state_ );
	}
};

struct PyObjDestructor {
	void operator()(PyObject *p)
	{
		Py_XDECREF( p );
	}
};

static inline PyObject *
PyObj(PyObject *v)
{
	if ( ! v ) {
		v = Py_None;
		Py_INCREF( v );
	}
	return ( v );
}

static inline PyObject *
PyObj(int64_t   v)
{
	return PyLong_FromLongLong( v );
}

static inline PyObject *
PyObj(uint64_t  v)
{
	return PyLong_FromUnsignedLongLong( v );
}

static inline PyObject *
PyObj(double    v)
{
	return PyFloat_FromDouble( v );
}

static inline PyObject *
PyObj(const std::string &v)
{
	return PyUnicode_FromString( v.c_str() );
}

class PyListObj;

class PyUniqueObj {
private:

typedef cpsw::unique_ptr<PyObject, PyObjDestructor> B;

	B up_;

public:
	PyUniqueObj()
	: up_( PyObj( (PyObject*)NULL ) )
	{}

	PyUniqueObj(PyListObj &o);

	template <typename T>
	PyUniqueObj(T o)
	: up_( PyObj( o ) )
	{
	}

	PyUniqueObj(PyUniqueObj &orig)
	: up_( cpsw::move( orig.up_ ) )
	{
	}

	PyUniqueObj & operator=(PyUniqueObj &rhs)
	{
		up_ = cpsw::move( rhs.up_ );
		return *this;
	}

	/* Some move-semantics hacks to accommodate
 *   * old compilers. This method allows the unique
 *   * pointer to be transferred from a temporary
 *   * object (such as a return value) to 'to'.
 *   */
    void
	transfer(PyUniqueObj &to)
	{
		to.up_ = cpsw::move( up_ );
	}

	PyObject *release()
	{
		return up_.release();
	}

	PyObject *get()
	{
		return up_.get();
	}

	operator bool()
	{
		return (bool)up_;
	}
};

class PyListObj : public PyUniqueObj {
public:
	PyListObj()
	: PyUniqueObj( PyList_New( 0 ) )
	{}

	template <typename T>
	void append( T a )
	{
		if ( PyList_Append( get(), PyObj( a ) ) ) {
			throw InternalError("Unable to append to python list");
		}
	}
};

// It is OK to release the GIL while holding
// a reference to the Py_buffer.
//
// I tested with python2.7.12 and python3.5 --
// when trying to resize a buffer (from python)
// which has an exported view this will be
// rejected:
//
// (numpy.array, python2.7)
// ValueError: cannot resize an array that references or is referenced
// by another array in this way.
//
// (bytearray, python3.5)
// BufferError: Existing exports of data: object cannot be re-sized
class ViewGuard {
private:
	Py_buffer *theview_;

public:
	ViewGuard(Py_buffer *view)
	: theview_(view)
	{
	}

	~ViewGuard()
	{
		PyBuffer_Release( theview_ );
	}
};

uint64_t
wrap_Path_loadConfigFromYamlFile(ConstPath p, const char *yamlFile,  const char *yaml_dir = 0);

uint64_t
IPath_loadConfigFromYamlFile(const IPath *p, const char *yamlFile,  const char *yaml_dir = 0);

uint64_t
wrap_Path_loadConfigFromYamlString(ConstPath p, const char *yaml,  const char *yaml_dir = 0);

uint64_t
IPath_loadConfigFromYamlString(const IPath *p, const char *yaml,  const char *yaml_dir = 0);

uint64_t
wrap_Path_dumpConfigToYamlFile(ConstPath p, const char *filename, const char *templFilename = 0, const char *yaml_dir = 0);

uint64_t
IPath_dumpConfigToYamlFile(const IPath *p, const char *filename, const char *templFilename = 0, const char *yaml_dir = 0);

std::string
wrap_Path_dumpConfigToYamlString(ConstPath p, const char *templ = 0, const char *yaml_dir = 0, bool templIsFilename = true);

std::string
IPath_dumpConfigToYamlString(const IPath *p, const char *templ = 0, const char *yaml_dir = 0, bool templIsFilename = true);

Path
wrap_Path_loadYamlStream(const std::string &yaml, const char *root_name = "root", const char *yaml_dir_name = 0, IYamlFixup *fixup = 0);

std::string
wrap_Val_Base_repr(shared_ptr<IVal_Base> obj);

std::string
IVal_Base_repr(IVal_Base *obj);

void
wrap_Command_execute(Command command);

void
ICommand_execute(ICommand *command);

unsigned
IScalVal_RO_getVal(IScalVal_RO *val, PyObject *o, int from = -1, int to = -1);

PyObject *
IScalVal_RO_getVal(IScalVal_RO *val, int fromIdx = -1, int toIdx = -1, bool forceNumeric = false);

unsigned
IScalVal_setVal(IScalVal *val, PyObject *op, int from = -1, int to = -1);

unsigned
IDoubleVal_setVal(IDoubleVal *val, PyObject *op, int from = -1, int to = -1);

PyObject *
IDoubleVal_RO_getVal(IDoubleVal_RO *val, int from = -1, int to = -1);


int64_t
IStream_read(IStream *val, PyObject *op, int64_t timeoutUs, uint64_t offset);

int64_t
IStream_write(IStream *val, PyObject *op, int64_t timeoutUs);

template <typename T, typename LT>
class CGetValWrapperContextTmpl {
private:
	typedef enum { NONE, STRING, SIGNED, UNSIGNED, DOUBLE } ValType;

	ValType               type_;
	unsigned              nelms_;
	std::vector<CString>  stringBuf_;
	std::vector<uint64_t> v64_;
	std::vector<double>   d64_;
	IndexRange            range_;

	CGetValWrapperContextTmpl(const CGetValWrapperContextTmpl &);
	CGetValWrapperContextTmpl & operator=(const CGetValWrapperContextTmpl&);

public:

	virtual void prepare(IVal_Base *val, int from, int to)
	{
		nelms_ = val->getNelms();
		range_ = IndexRange( from, to );

		if ( from >= 0 || to >= 0 ) {
			// index range may reduce nelms
			SlicedPathIterator it( val->getPath(), &range_ );
			nelms_ = it.getNelmsLeft();
		}
	}

	virtual T complete(CPSWError *err)
	{
		if ( err ) {
			T rval;
			return rval;
		}
		if ( STRING == type_ ) {
			if ( 1 == nelms_ ) {
				T rval( *stringBuf_[0] );
				return rval;
			}

			LT l;
			for ( unsigned i = 0; i<nelms_; i++ ) {
				l.append( *stringBuf_[i] );
			}
			return l;
		} else if ( DOUBLE == type_ ) {
			if ( 1 == nelms_ ) {
				T rval( d64_[0] );
				return rval;
			}

			LT l;
			for ( unsigned i = 0; i<nelms_; i++ ) {
				l.append( d64_[i] );
			}
			return l;
		} else {
			if ( 1 == nelms_ ) {
				if ( SIGNED == type_ ) {
					int64_t ival = (int64_t)v64_[0];
					T rval( ival );
					return rval;
				} else {
					T rval( v64_[0] );
					return rval;
				}
			}

			LT l;
			if ( SIGNED == type_ ) {
				for ( unsigned i = 0; i<nelms_; i++ ) {
					int64_t ival = (int64_t)v64_[i];
					l.append( ival );
				}
			} else {
				for ( unsigned i = 0; i<nelms_; i++ ) {
					l.append( v64_[i] );
				}
			}
			return l;
		}
	}

	virtual unsigned issueGetVal(IScalVal_RO *val, int from, int to, bool forceNumeric, AsyncIO aio)
	{
	GILUnlocker allowThreadingWhileWaiting;

		prepare( val, from , to );

		if ( ! forceNumeric && val->getEnum() ) {
			type_ = STRING;
		} else {
			type_ = val->isSigned() ? SIGNED : UNSIGNED;
		}

		if ( STRING == type_ ) {
			// must not use 'reserve' which doesn't construct invalid shared pointers!
			stringBuf_ = std::vector<CString>( nelms_, CString() );

			if ( aio ) {
				return val->getVal( aio, &stringBuf_[0], nelms_, &range_ );
			} else {
				return val->getVal( &stringBuf_[0], nelms_, &range_ );
			}
		} else {
			v64_.reserve( nelms_ );

			if ( aio ) {
				return val->getVal( aio, &v64_[0], nelms_, &range_ );
			} else {
				return val->getVal( &v64_[0], nelms_, &range_ );
			}
		}
	}

	virtual unsigned issueGetVal(IDoubleVal_RO *val, int from, int to, AsyncIO aio)
	{
	GILUnlocker allowThreadingWhileWaiting;

		prepare( val, from , to );
		type_ = DOUBLE;
		d64_.reserve( nelms_ );

		if ( aio ) {
			return val->getVal( aio, &d64_[0], nelms_, &range_ );
		} else {
			return val->getVal( &d64_[0], nelms_, &range_ );
		}
	}

	CGetValWrapperContextTmpl()
	: type_ ( NONE ),
	  nelms_( 0    ),
	  range_( -1, -1 )
	{
	}

		~CGetValWrapperContextTmpl()
	{
	}
};

};

#endif
