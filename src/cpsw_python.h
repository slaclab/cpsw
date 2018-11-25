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
#include <Python.h>
#include <string>

namespace cpsw_python {

class GILUnlocker {
private:
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

struct PyObjDestructor {
	void operator()(PyObject *p)
	{
		Py_DECREF( p );
	}
};

typedef std::unique_ptr<PyObject, PyObjDestructor> UniquePyObj;

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
wrap_Path_loadConfigFromYamlFile(Path p, const char *yamlFile,  const char *yaml_dir = 0);

uint64_t
IPath_loadConfigFromYamlFile(IPath *p, const char *yamlFile,  const char *yaml_dir = 0);

uint64_t
wrap_Path_loadConfigFromYamlString(Path p, const char *yaml,  const char *yaml_dir = 0);

uint64_t
IPath_loadConfigFromYamlString(IPath *p, const char *yaml,  const char *yaml_dir = 0);

uint64_t
wrap_Path_dumpConfigToYamlFile(Path p, const char *filename, const char *templFilename = 0, const char *yaml_dir = 0);

uint64_t
IPath_dumpConfigToYamlFile(IPath *p, const char *filename, const char *templFilename = 0, const char *yaml_dir = 0);

std::string
wrap_Path_dumpConfigToYamlString(Path p, const char *templFilename = 0, const char *yaml_dir = 0);

std::string
IPath_dumpConfigToYamlString(IPath *p, const char *templFilename = 0, const char *yaml_dir = 0);

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

};

#endif
