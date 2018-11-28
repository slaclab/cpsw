 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef SWIG_PYTHON_H
#define SWIG_PYTHON_H

#include <cpsw_api_user.h>
#include <Python.h>

namespace cpsw_python {
	// To be used from a 'catch' block
	void handleException();

	template <typename, typename> class CGetValWrapperContextTmpl;
	class PyUniqueObj;
	class PyListObj;

	typedef CGetValWrapperContextTmpl<PyUniqueObj, PyListObj> CGetValWrapperContext;

};

// The following are not recognized by 'rename' (if I use the 'ignore all'
// method) when in the cpsw_python namespace :-(

class CAsyncIOWrapper : public IAsyncIO {
private:
	CAsyncIOWrapper(const CAsyncIOWrapper&);

	CAsyncIOWrapper &
	operator=(const CAsyncIOWrapper&);

	std::unique_ptr<
		cpsw_python::CGetValWrapperContextTmpl<
			cpsw_python::PyUniqueObj,
			cpsw_python::PyListObj            >
                   > ctxt_;

public:
	CAsyncIOWrapper();

	virtual void py_callback(PyObject *) = 0;

	virtual void callback(CPSWError *);

	virtual ~CAsyncIOWrapper();
};

PyObject *
IScalVal_RO_getVal(IScalVal_RO *val, int fromIdx = -1, int toIdx = -1, bool forceNumeric = false);

void cpswSwigRegisterExceptions(PyObject *module);

#endif

