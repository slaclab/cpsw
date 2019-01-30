 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* Generic python helpers */

#include <cpsw_api_user.h>
#include <cpsw_yaml.h>
#include <cpsw_python.h>
#include <Python.h>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace cpsw_python {

uint64_t
wrap_Path_loadConfigFromYamlFile(ConstPath p, const char *yamlFile,  const char *yaml_dir)
{
	return IPath_loadConfigFromYamlFile(p.get(), yamlFile, yaml_dir);
}

uint64_t
IPath_loadConfigFromYamlFile(const IPath *p, const char *yamlFile,  const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
	return p->loadConfigFromYamlFile( yamlFile, yaml_dir );
}

uint64_t
wrap_Path_loadConfigFromYamlString(ConstPath p, const char *yaml,  const char *yaml_dir)
{
	return IPath_loadConfigFromYamlString(p.get(), yaml, yaml_dir);
}

uint64_t
IPath_loadConfigFromYamlString(const IPath *p, const char *yaml,  const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
YAML::Node  conf( CYamlFieldFactoryBase::loadPreprocessedYaml( yaml, yaml_dir ) );
	return p->loadConfigFromYaml( conf );
}

uint64_t
wrap_Path_dumpConfigToYamlFile(ConstPath p, const char *filename, const char *templFilename, const char *yaml_dir)
{
	return IPath_dumpConfigToYamlFile(p.get(), filename, templFilename, yaml_dir);
}

uint64_t
IPath_dumpConfigToYamlFile(const IPath *p, const char *filename, const char *templFilename, const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
uint64_t    rval;
YAML::Node  conf;

	if ( templFilename ) {
		conf = CYamlFieldFactoryBase::loadPreprocessedYamlFile( templFilename, yaml_dir );
	}

	rval = p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

std::fstream strm( filename, std::fstream::out );
	strm << emit.c_str() << "\n";

	return rval;
}

std::string
wrap_Path_dumpConfigToYamlString(ConstPath p, const char *templFilename, const char *yaml_dir)
{
	return	IPath_dumpConfigToYamlString(p.get(), templFilename, yaml_dir);
}

std::string
IPath_dumpConfigToYamlString(const IPath *p, const char *templFilename, const char *yaml_dir)
{
GILUnlocker allowThreadingWhileWaiting;
YAML::Node  conf;

	if ( templFilename ) {
		conf = CYamlFieldFactoryBase::loadPreprocessedYamlFile( templFilename, yaml_dir );
	}

	p->dumpConfigToYaml( conf );

YAML::Emitter emit;
	emit << conf;

std::ostringstream strm;

	strm << emit.c_str() << "\n";

	return strm.str();
}

std::string
wrap_Val_Base_repr(shared_ptr<IVal_Base> obj)
{
	return IVal_Base_repr(obj.get());
}

std::string
IVal_Base_repr(IVal_Base *obj)
{
	return obj->getPath()->toString();
}

Path
wrap_Path_loadYamlStream(const std::string &yaml, const char *root_name, const char *yaml_dir_name, IYamlFixup *fixup)
{
// could use IPath::loadYamlStream(const char *,...) but that would make a new string
// which we want to avoid.
std::istringstream sstrm( yaml );
	return IPath::loadYamlStream( sstrm, root_name, yaml_dir_name, fixup );
}

void
wrap_Command_execute(Command command)
{
	ICommand_execute( command.get() );
}

void
ICommand_execute(ICommand *command)
{
GILUnlocker allowThreadingWhileWaiting;
	command->execute();
}

unsigned
IScalVal_RO_getVal(IScalVal_RO *val, PyObject *op, int from, int to)
{
Py_buffer  view;
IndexRange rng(from, to);

	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS | PyBUF_WRITEABLE ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	Py_ssize_t nelms = view.len / view.itemsize;

	if ( nelms > val->getNelms() )
		nelms = val->getNelms();

	{
	GILUnlocker allowThreadingWhileWaiting;
	// Nobody else must mess with the buffer while the GIL is unlocked!

	if        ( view.itemsize == sizeof(uint8_t ) ) {
		uint8_t *bufp = reinterpret_cast<uint8_t*>(view.buf);
		// set same value to all elements ?
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint16_t) ) {
		uint16_t *bufp = reinterpret_cast<uint16_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint32_t) ) {
		uint32_t *bufp = reinterpret_cast<uint32_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	} else if ( view.itemsize == sizeof(uint64_t) ) {
		uint64_t *bufp = reinterpret_cast<uint64_t*>(view.buf);
		return val->getVal( bufp, nelms, &rng );
	}
	}

	throw InvalidArgError("Unable to convert python argument");
}

PyObject *
IScalVal_RO_getVal(IScalVal_RO *val, int fromIdx, int toIdx, bool forceNumeric)
{
	/* Need to hack around in order to get the shared pointer back... */
	CGetValWrapperContextTmpl<PyUniqueObj, PyListObj> ctxt;
        ctxt.issueGetVal( val, fromIdx, toIdx, forceNumeric, AsyncIO() );
	PyUniqueObj o = std::move( ctxt.complete( 0 ) );
	return o.release();
}

unsigned
IScalVal_setVal(IScalVal *val, PyObject *op, int from, int to)
{
Py_buffer  view;
IndexRange rng(from, to);

bool useEnum    = !!val->getEnum();
bool enumScalar = false;

	// use buffer interface only for non-enums

	if ( useEnum ) {
		if ( ! PyBytes_Check( op ) && ! PyUnicode_Check( op ) ) {
			if ( PySequence_Check( op ) ) {
				// could be a sequence of strings
				PyUniqueObj o1( PySequence_GetItem( op, 0 ) );
				PyObject *op1 = o1.get();
				if ( ! PyBytes_Check( op1 ) && ! PyUnicode_Check( op1 ) ) {
					// not a sequence of strings - do not use enums
					useEnum = false;
				}
			} else {
				// a scalar that is not a string cannot use enums
				useEnum = false;
			}
		} else {
			enumScalar = true;
		}
	}

	if (  ! useEnum
		 && PyObject_CheckBuffer( op )
	     && 0 == PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS ) ) {
		// new style buffer interface
		ViewGuard guard( &view );

		Py_ssize_t nelms = view.len / view.itemsize;

		{
		GILUnlocker allowThreadingWhileWaiting;
		if        ( view.itemsize == sizeof(uint8_t ) ) {
			uint8_t *bufp = reinterpret_cast<uint8_t*>(view.buf);
			// set same value to all elements ?
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint16_t) ) {
			uint16_t *bufp = reinterpret_cast<uint16_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint32_t) ) {
			uint32_t *bufp = reinterpret_cast<uint32_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		} else if ( view.itemsize == sizeof(uint64_t) ) {
			uint64_t *bufp = reinterpret_cast<uint64_t*>(view.buf);
			return 1==nelms ? val->setVal( (uint64_t)*bufp ) : val->setVal( bufp, nelms, &rng );
		}
		}

		// if we get here then we couldn't deal with the buffer interface;
		// try the hard way...
	}

	// if the object is not a python sequence then 'len()' cannot be used.
	// Thus, we have to handle this case separately...

	if ( ! PySequence_Check( op ) ) {
		// a single string (attempt to set enum) is also a sequence
		// boost::python::extract() is not very useful here
		// since it does a range check (and produces a mysterious
		// segfault [g++-5.4/python3.5] if the number if out of
		// range. We really want modulo 2^64 here which is what
		// the PyLong_AsUnsignedLongLongMask() achieves.
		uint64_t num64;
		if ( ! PyLong_Check( op ) ) {
#if PY_VERSION_HEX < 0x03000000
			if ( PyInt_Check( op ) ) {
				num64 = PyInt_AsUnsignedLongLongMask( op );
			} else 
#endif
			throw InvalidArgError("int/long value expected");
		} else {
        	num64 = PyLong_AsUnsignedLongLongMask( op );
		}
		{
			GILUnlocker allowThreadingWhileWaiting;
			return val->setVal( num64, &rng );
		}
	}

	long    seqLen = PySequence_Length( op );

	unsigned nelms = enumScalar ? 1 : ( seqLen > 0 ? seqLen : 0 );

	if ( useEnum ) {
		if ( enumScalar ) {
			PyUniqueObj bytes( PyUnicode_AsASCIIString( op ) );
			const char *str   = bytes ? PyBytes_AsString( bytes.get() ) : 0;
			if ( ! str ) {
				throw InvalidArgError("IScalVal_setVal: Unable to convert string to ASCII");
			}
			{
				GILUnlocker allowThreadingWhileWaiting;
				return val->setVal( str, &rng );
			}
		} else {
			// we can't just extract a 'const char *' pointer -- under
			// python3 all strings are unicode and we must first convert
			// to a c++ std::string. Should be backwards compatible to
			// python 2.7.
			std::vector<PyUniqueObj>  vstr;
			std::vector<const char *> vcstr;

			for ( unsigned i = 0; i < nelms; ++i ) {
				PyUniqueObj bytes( PyUnicode_AsASCIIString( PySequence_GetItem( op, i ) ) );
				const char *str = bytes ? PyBytes_AsString( bytes.get() ) : 0;
				if ( ! str ) {
					throw InvalidArgError("IScalVal_setVal: Unable to convert string to ASCII");
				}
				vstr.push_back( std::move( bytes ) );
				vcstr.push_back( str   );
			}
			{
				GILUnlocker allowThreadingWhileWaiting;
				return val->setVal( &vcstr[0], nelms, &rng );
			}
		}
	} else {
		std::vector<uint64_t> v64;
		for ( unsigned i = 0; i < nelms; ++i ) {
			PyUniqueObj ob_tmp( PySequence_GetItem( op, i ) );
			PyObject *op1 = ob_tmp.get();
			// see above why we use this instead of boost::python::extract
			uint64_t num64;
			if ( ! PyLong_Check( op1 ) ) {
#if PY_VERSION_HEX < 0x03000000
				if ( PyInt_Check( op1 ) ) {
					num64 = PyInt_AsUnsignedLongLongMask( op1 );
				} else 
#endif
				throw InvalidArgError("int/long value expected");
			} else {
				num64 = PyLong_AsUnsignedLongLongMask( op1 );
			}
			v64.push_back( num64 );
		}
		{
		GILUnlocker allowThreadingWhileWaiting;
		return val->setVal( &v64[0], nelms, &rng );
		}
	}
}

static inline double xtractDouble(PyObject *op)
{
	if ( ! PyFloat_Check( op ) ) {
		throw InvalidArgError("float object expected");
	}
	return PyFloat_AsDouble( op );
}

PyObject *
IDoubleVal_RO_getVal(IDoubleVal_RO *val, int fromIdx, int toIdx)
{
	/* Need to hack around in order to get the shared pointer back... */
	CGetValWrapperContextTmpl<PyUniqueObj, PyListObj> ctxt;
        ctxt.issueGetVal( val, fromIdx, toIdx, AsyncIO() );
	PyUniqueObj o = std::move( ctxt.complete( 0 ) );
	return o.release();
}


unsigned
IDoubleVal_setVal(IDoubleVal *val, PyObject *op, int from, int to)
{
IndexRange rng(from, to);

	// if the object is not a python sequence then 'len()' cannot be used.
	// Thus, we have to handle this case separately...

	if ( ! PySequence_Check( op ) ) {
		// a single string (attempt to set enum) is also a sequence
		GILUnlocker allowThreadingWhileWaiting;
		return val->setVal( xtractDouble( op ), &rng );
	}

	unsigned nelms = PySequence_Length( op );

	std::vector<double> v64;
	for ( unsigned i = 0; i < nelms; ++i ) {
		v64.push_back( xtractDouble( PySequence_GetItem( op, i ) ) );
	}
	{
	GILUnlocker allowThreadingWhileWaiting;
	return val->setVal( &v64[0], nelms, &rng );
	}
}


int64_t
IStream_read(IStream *val, PyObject *op, int64_t timeoutUs, uint64_t offset)
{
Py_buffer view;
	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS | PyBUF_WRITEABLE ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	CTimeout timeout;

	if ( timeoutUs >= 0 )
		timeout.set( (uint64_t)timeoutUs );

	{
	// hopefully it's OK to release the GIL while operating on the buffer view...
	GILUnlocker allowThreadingWhileWaiting;
		return val->read( reinterpret_cast<uint8_t*>(view.buf), view.len, timeout, offset );
	}
}

int64_t
IStream_write(IStream *val, PyObject *op, int64_t timeoutUs)
{
Py_buffer view;
	if ( !  PyObject_CheckBuffer( op )
	     || 0 != PyObject_GetBuffer( op, &view, PyBUF_C_CONTIGUOUS ) ) {
		throw InvalidArgError("Require an object which implements the buffer interface");
	}
	ViewGuard guard( &view );

	CTimeout timeout;
	if ( timeoutUs >= 0 )
		timeout.set( (uint64_t)timeoutUs );

	{
	// hopefully it's OK to release the GIL while operating on the buffer view...
	GILUnlocker allowThreadingWhileWaiting;
	return val->write( reinterpret_cast<uint8_t*>(view.buf), view.len, timeout );
	}
}

PyUniqueObj::PyUniqueObj( PyListObj &o )
: B( o.release() )
{
}

void
handleException()
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
	catch ( std::exception &e ) {
		PyErr_SetString( PyExc_RuntimeError, e.what() );
	}
}

};
