#ifndef SWIG_PYTHON_H
#define SWIG_PYTHON_H

#include <cpsw_api_user.h>
#include <Python.h>

namespace cpsw_python {
	// To be used from a 'catch' block
	void handleException();

	void registerExceptions(PyObject *module);
};

#endif

