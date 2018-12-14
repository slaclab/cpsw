#cython: embedsignature=True

 #@C Copyright Notice
 #@C ================
 #@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 #@C file found in the top-level directory of this distribution and at
 #@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 #@C
 #@C No part of CPSW, including this file, may be copied, modified, propagated, or
 #@C distributed except according to the terms contained in the LICENSE.txt file.

from cpython.object cimport PyObject
from libcpp.memory  cimport shared_ptr
from libcpp         cimport bool
from libcpp.vector  cimport *
from libcpp.string  cimport *
from libc.stdint    cimport *
from libc.stdio     cimport *

cdef extern from "cpsw_api_user.h" namespace "YAML":
  cdef cppclass Node

cdef extern from "cpsw_api_user.h":
  cdef cppclass IEntry
  cdef cppclass IChild
  cdef cppclass IHub
  cdef cppclass IPath

cdef extern from "cpsw_python.h" namespace "cpsw_python":
  cdef void handleException()

  ctypedef const IEntry CIEntry
  ctypedef const IChild CIChild
  ctypedef const IHub   CIHub   
  ctypedef const IPath  CIPath
  ctypedef shared_ptr[CIEntry] cc_Entry
  ctypedef shared_ptr[CIChild] cc_Child
  ctypedef shared_ptr[ vector[cc_Child] ] cc_Children
  ctypedef shared_ptr[CIHub]   cc_Hub
  ctypedef shared_ptr[IPath  ] cc_Path
  ctypedef shared_ptr[CIPath ] cc_ConstPath
  ctypedef CIEntry            *CIEntryp
  ctypedef CIChild            *CIChildp
  ctypedef CIHub              *CIHubp
  ctypedef CIPath             *CIPathp
  ctypedef IPath              *IPathp

cdef extern from "cpsw_api_user.h":

  cdef cppclass IYamlFixup:
    @staticmethod
    Node findByName(Node &src, const char *path, char sep)

  cdef cppclass IPathVisitor:
    pass

  cdef cppclass IEntry:
    const char *getName() except+handleException
    uint64_t    getSize() except+handleException
    const char *getDescription() except+handleException
    double      getPollSecs() except+handleException
    cc_Hub      isHub() except+handleException

  cdef cppclass IChild(IEntry):
    cc_Hub      getOwner() except+handleException
    unsigned    getNelms() except+handleException

  cdef cppclass IHub(IEntry):
    cc_Path     findByName(const char*) except+handleException
    cc_Child    getChild(const char *) except+handleException
    cc_Children getChildren() except+handleException

  cdef cppclass IPath:
    cc_Path     findByName(const char *) except+handleException
    cc_Child    up() except+handleException
    bool        empty() except+handleException
    int         size() except+handleException
    void        clear() except+handleException
    void        clear(cc_Hub) except+handleException
    cc_Hub      origin() except+handleException
    cc_Hub      parent() except+handleException
    cc_Child    tail() except+handleException
    string      toString() except+handleException
    void        dump(FILE*) except+handleException
    bool        verifyAtTail(cc_Path) except+handleException
    void        append(cc_Path) except+handleException
    cc_Path     concat(cc_Path) except+handleException
    cc_Path     clone() except+handleException
    cc_Path     intersect(cc_Path) except+handleException
    bool        isIntersecting(cc_Path) except+handleException
    unsigned    getNelms() except+handleException
    unsigned    getTailFrom() except+handleException
    unsigned    getTailTo() except+handleException
    void        explore(IPathVisitor *) except+handleException
    uint64_t    dumpConfigToYaml(Node &) except+handleException
    uint64_t    loadConfigFromYaml(Node &) except+handleException
    uint64_t    loadConfigFromYamlFile(Node &) except+handleException

#	# Overloading doesn't seem to work properly :-(
    @staticmethod
    cc_Path     create0 "create"() except+handleException

    @staticmethod
    cc_Path     create1 "create"(cc_Hub) except+handleException

    @staticmethod
    cc_Path     create2 "create"(const char *) except+handleException

    @staticmethod
    cc_Path     loadYamlFile(const char *, const char *, const char *, IYamlFixup*) except+handleException

    @staticmethod
    cc_Path     loadYamlStream(const char *, const char *, const char *, IYamlFixup*) except+handleException

cdef extern from "cpsw_cython.h" namespace "cpsw_python":
  cdef cppclass CPathVisitor:
    CPathVisitor()
    CPathVisitor(PyObject *o)

  cdef cppclass CYamlFixup:
    CYamlFixup()
    CYamlFixup(PyObject *o)
