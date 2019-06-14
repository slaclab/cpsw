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
#from libcpp.memory  cimport shared_ptr namespace "cpsw"
#from libcpp.memory  cimport make_shared
from libcpp         cimport bool
from libcpp.vector  cimport *
from libcpp.string  cimport *
from libc.stdint    cimport *
from libc.stdio     cimport *

from yaml_cpp       cimport c_Node

# Redefine stuff from libcpp.memory so we can
# use our 'cpsw' namespace and redirect to boost
# if we don't have c++11...
cdef extern from "<cpsw_shared_ptr.h>" nogil:
  cdef cppclass shared_ptr[T]:
    T   *get()
    void reset()
    bool operator bool()
    bool operator!()

cdef extern from "<cpsw_shared_ptr.h>" namespace "cpsw" nogil:
  cdef shared_ptr[T] make_shared[T](...) except +

cdef extern from "cpsw_api_user.h":
  cdef cppclass IEntry
  cdef cppclass IChild
  cdef cppclass IHub
  cdef cppclass IPath
  cdef cppclass IVal_Base
  cdef cppclass IScalVal_Base
  cdef cppclass IEnum
  cdef cppclass IAsyncIO
  cdef cppclass IScalVal_RO
  cdef cppclass IScalVal
  cdef cppclass IStream
  cdef cppclass ICommand
  cdef cppclass IDoubleVal_RO
  cdef cppclass IDoubleVal

ctypedef const IEntry            CIEntry
ctypedef const IChild            CIChild
ctypedef const IHub              CIHub
ctypedef const IPath             CIPath
ctypedef const IVal_Base         CIVal_Base
ctypedef const IScalVal_Base     CIScalVal_Base
ctypedef const IEnum             CIEnum
ctypedef const string            CString

ctypedef shared_ptr[CIEntry]                 cc_ConstEntry
ctypedef shared_ptr[IEntry]                  cc_Entry
ctypedef shared_ptr[CIChild]                 cc_ConstChild
ctypedef shared_ptr[IChild]                  cc_Child
ctypedef shared_ptr[ vector[cc_ConstChild] ] cc_ConstChildren
ctypedef shared_ptr[CIHub]                   cc_ConstHub
ctypedef shared_ptr[IHub]                    cc_Hub
ctypedef shared_ptr[IPath  ]                 cc_Path
ctypedef shared_ptr[CIPath ]                 cc_ConstPath
ctypedef shared_ptr[IVal_Base]               cc_Val_Base
ctypedef shared_ptr[CIVal_Base]              cc_ConstVal_Base
ctypedef shared_ptr[IScalVal_Base]           cc_ScalVal_Base
ctypedef shared_ptr[CIScalVal_Base]          cc_ConstScalVal_Base
ctypedef shared_ptr[IEnum]                   cc_Enum
ctypedef shared_ptr[CIEnum]                  cc_ConstEnum
ctypedef shared_ptr[CString]                 cc_ConstString
ctypedef shared_ptr[IAsyncIO]                cc_AsyncIO
ctypedef shared_ptr[IScalVal_RO]             cc_ScalVal_RO
ctypedef shared_ptr[IScalVal]                cc_ScalVal
ctypedef shared_ptr[IStream]                 cc_Stream
ctypedef shared_ptr[ICommand]                cc_Command
ctypedef shared_ptr[IDoubleVal_RO]           cc_DoubleVal_RO
ctypedef shared_ptr[IDoubleVal]              cc_DoubleVal

cdef extern from "cpsw_python.h" namespace "cpsw_python":
  cdef void handleException()
  cdef uint64_t wrap_Path_loadConfigFromYamlFile  (cc_Path, const char *,  const char *)           except+handleException
  cdef uint64_t wrap_Path_loadConfigFromYamlString(cc_Path, const char *,  const char *)           except+handleException

  cdef uint64_t wrap_Path_dumpConfigToYamlFile(cc_Path, const char *, const char *, const char *)  except+handleException
  cdef string   wrap_Path_dumpConfigToYamlString(cc_Path, const char *, const char *, bool)        except+handleException

  cdef cc_Path  wrap_Path_loadYamlStream(const string &, const char *, const char *, IYamlFixup *) except+handleException
  cdef unsigned IScalVal_RO_getVal(IScalVal_RO *, PyObject *, int , int )                          except+handleException

  cdef int64_t  IStream_read (IStream *, PyObject *, int64_t, uint64_t)                            except+handleException
  cdef int64_t  IStream_write(IStream *, PyObject *, int64_t timeoutUs)                            except+handleException

  cdef PyObject * IScalVal_RO_getVal(IScalVal_RO *, int, int, bool)                                except+handleException
  cdef PyObject * IScalVal_RO_getVal(IScalVal_RO *, PyObject *, int, int, bool)                    except+handleException
  cdef unsigned   IScalVal_setVal(IScalVal *, PyObject *, int, int)                                except+handleException
  cdef PyObject * IDoubleVal_RO_getVal(IDoubleVal_RO *, int, int)                                  except+handleException
  cdef unsigned   IDoubleVal_setVal(IDoubleVal *, PyObject *, int, int)                            except+handleException

cdef extern from "cpsw_api_user.h":
  cdef cppclass   iterator "Children::element_type::const_iterator":
    iterator     &operator++()           except+;
    cc_ConstChild operator*()            except+;
    bool          operator!=(iterator &) except+;

cdef extern from "cpsw_api_user.h" namespace "IEnum":
  cdef struct EnumItem     "IEnum::Item":
    cc_ConstString first
    uint64_t       second

  cdef cppclass EnumIterator "IEnum::iterator":
    bool operator!=(const EnumIterator &)
    EnumIterator &operator++()
    EnumItem      operator*()
    

cdef extern from "cpsw_api_user.h" namespace "IVal_Base":
  enum Encoding:
    pass
  ctypedef Encoding ValEncoding "IVal_Base::Encoding"


cdef extern from "cpsw_api_user.h":

  cdef cppclass IYamlFixup:
    @staticmethod
    c_Node findByName(c_Node &src, const char *path, char sep) except+handleException

  cdef cppclass IPathVisitor:
    pass

  cdef cppclass IEntry:
    const char *getName()        except+handleException
    uint64_t    getSize()        except+handleException
    const char *getDescription() except+handleException
    double      getPollSecs()    except+handleException
    cc_ConstHub      isHub()          except+handleException

  cdef cppclass IChild(IEntry):
    cc_ConstHub getOwner()       except+handleException
    unsigned    getNelms()       except+handleException

  cdef cppclass IHub(IEntry):
    cc_Path          findByName(const char*)  except+handleException
    cc_ConstChild    getChild(const char *)   except+handleException
    cc_ConstChildren getChildren()            except+handleException

  cdef cppclass IPath:
    cc_Path       findByName(const char *)         except+handleException
    cc_ConstChild up()                             except+handleException
    bool          empty()                          except+handleException
    int           size()                           except+handleException
    void          clear()                          except+handleException
    void          clear(cc_Hub)                    except+handleException
    cc_ConstHub   origin()                         except+handleException
    cc_ConstHub   parent()                         except+handleException
    cc_ConstChild tail()                           except+handleException
    string        toString()                       except+handleException
    void          dump(FILE*)                      except+handleException
    bool          verifyAtTail(cc_Path)            except+handleException
    void          append(cc_Path)                  except+handleException
    cc_Path       concat(cc_Path)                  except+handleException
    cc_Path       clone()                          except+handleException
    cc_Path       intersect(cc_Path)               except+handleException
    bool          isIntersecting(cc_Path)          except+handleException
    unsigned      getNelms()                       except+handleException
    unsigned      getTailFrom()                    except+handleException
    unsigned      getTailTo()                      except+handleException
    void          explore(IPathVisitor *)          except+handleException
    uint64_t      dumpConfigToYaml(c_Node &)       except+handleException
    uint64_t      loadConfigFromYaml(c_Node &)     except+handleException
    uint64_t      loadConfigFromYamlFile(c_Node &) except+handleException

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

  cdef cppclass IVal_Base(IEntry):
    unsigned     getNelms()                     except+handleException
    cc_Path      getPath()                      except+handleException
    cc_ConstPath getConstPath()                 except+handleException
    ValEncoding  getEncoding()                  except+handleException

    @staticmethod
    cc_Val_Base create(cc_Path path)            except+handleException

  cdef cppclass IEnum:
    EnumIterator begin()                        except+handleException
    EnumIterator end()                          except+handleException
    unsigned     getNelms()                     except+handleException

  cdef cppclass IScalVal_Base(IVal_Base):
    uint64_t     getSizeBits()                  except+handleException
    bool         isSigned()                     except+handleException
    cc_Enum      getEnum()                      except+handleException

    @staticmethod
    cc_ScalVal_Base create(cc_Path path)        except+handleException

  cdef cppclass IAsyncIO:
    pass

  cdef cppclass IScalVal_RO(IScalVal_Base):
    @staticmethod
    cc_ScalVal_RO create(cc_Path path)          except+handleException

  cdef cppclass IScalVal(IScalVal_RO):
    @staticmethod
    cc_ScalVal create(cc_Path path)             except+handleException

  cdef cppclass IStream(IVal_Base):
    @staticmethod
    cc_Stream  create(cc_Path path)             except+handleException

  cdef cppclass ICommand(IVal_Base):
    void       execute()                        except+handleException
    cc_Path    getPath()                        except+handleException
    @staticmethod
    cc_Command create(cc_Path path)             except+handleException

  cdef cppclass IDoubleVal_RO(IVal_Base):
    @staticmethod
    cc_DoubleVal_RO create(cc_Path path)        except+handleException

  cdef cppclass IDoubleVal(IDoubleVal_RO):
    @staticmethod
    cc_DoubleVal create(cc_Path path)           except+handleException

  cdef const char *c_getCPSWVersionString "getCPSWVersionString"()              except+handleException
  cdef int         c_setCPSWVerbosity     "setCPSWVerbosity"(const char *, int) except+handleException


cdef extern from "cpsw_cython.h" namespace "cpsw_python":
  cdef cppclass CPathVisitor(IPathVisitor):
    CPathVisitor()            except+handleException
    CPathVisitor(PyObject *o) except+handleException

  cdef cppclass CYamlFixup(IYamlFixup):
    CYamlFixup()              except+handleException
    CYamlFixup(PyObject *o)   except+handleException

  cdef cppclass CAsyncIO(IAsyncIO):
    CAsyncIO(PyObject *o)     except+handleException
    CAsyncIO()                except+handleException
    void       init(PyObject*)                                       except+handleException
    cc_AsyncIO makeShared()                                          except+handleException
    unsigned   issueGetVal(IScalVal_RO*, int, int, bool, cc_AsyncIO) except+handleException
    unsigned   issueGetVal(IDoubleVal_RO *, int, int, cc_AsyncIO)    except+handleException

  cdef c_Node wrap_IYamlFixup_findByName(const c_Node &, const char *, char) except+handleException

cdef extern from "cpsw_error.h":
  cdef cppclass cc_CPSWError "CPSWError":
    cc_CPSWError(const char*)
    const char *what()
  cdef cppclass cc_NoError "NoError"(cc_CPSWError):
    cc_NoError(const char *)
  cdef cppclass cc_DuplicateNameError "DuplicateNameError" (cc_CPSWError):
    cc_DuplicateNameError(const char *)
  cdef cppclass cc_NotDevError "NotDevError" (cc_CPSWError):
    cc_NotDevError(const char *)
  cdef cppclass cc_NotFoundError "NotFoundError" (cc_CPSWError):
    cc_NotFoundError(const char *)
  cdef cppclass cc_InvalidPathError "InvalidPathError" (cc_CPSWError):
    cc_InvalidPathError(const char *)
  cdef cppclass cc_InvalidIdentError "InvalidIdentError" (cc_CPSWError):
    cc_InvalidIdentError(const char *)
  cdef cppclass cc_InvalidArgError "InvalidArgError" (cc_CPSWError):
    cc_InvalidArgError(const char *)
  cdef cppclass cc_AddressAlreadyAttachedError "AddressAlreadyAttachedError" (cc_CPSWError):
    cc_AddressAlreadyAttachedError(const char *)
  cdef cppclass cc_ConfigurationError "ConfigurationError" (cc_CPSWError):
    cc_ConfigurationError(const char *)
  cdef cppclass cc_ErrnoError "ErrnoError" (cc_CPSWError):
    cc_ErrnoError(const char *)
  cdef cppclass cc_InternalError "InternalError" (cc_ErrnoError):
    cc_InternalError(const char *)
  cdef cppclass cc_AddrOutOfRangeError "AddrOutOfRangeError" (cc_CPSWError):
    cc_AddrOutOfRangeError(const char *)
  cdef cppclass cc_ConversionError "ConversionError" (cc_CPSWError):
    cc_ConversionError(const char *)
  cdef cppclass cc_InterfaceNotImplementedError "InterfaceNotImplementedError" (cc_CPSWError):
    cc_InterfaceNotImplementedError(const char *)
  cdef cppclass cc_IOError "IOError" (cc_ErrnoError):
    cc_IOError(const char *)
  cdef cppclass cc_BadStatusError "BadStatusError" (cc_CPSWError):
    cc_BadStatusError(const char *)
  cdef cppclass cc_IntrError "IntrError" (cc_CPSWError):
    cc_IntrError(const char *)
  cdef cppclass cc_StreamDoneError "StreamDoneError" (cc_CPSWError):
    cc_StreamDoneError (const char *)
  cdef cppclass cc_FailedStreamError "FailedStreamError" (cc_CPSWError):
    cc_FailedStreamError (const char *)
  cdef cppclass cc_MissingOnceTagError "MissingOnceTagError" (cc_CPSWError):
    cc_MissingOnceTagError (const char *)
  cdef cppclass cc_MissingIncludeFileNameError "MissingIncludeFileNameError" (cc_CPSWError):
    cc_MissingIncludeFileNameError (const char *)
  cdef cppclass cc_NoYAMLSupportError "NoYAMLSupportError" (cc_CPSWError):
    cc_NoYAMLSupportError (const char *)
  cdef cppclass cc_MultipleInstantiationError "MultipleInstantiationError" (cc_CPSWError):
    cc_MultipleInstantiationError (const char *)
  cdef cppclass cc_BadSchemaVersionError "BadSchemaVersionError" (cc_CPSWError):
    cc_BadSchemaVersionError (const char *)
  cdef cppclass cc_TimeoutError "TimeoutError" (cc_CPSWError):
    cc_TimeoutError (const char *)
