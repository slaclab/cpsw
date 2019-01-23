 #@C Copyright Notice
 #@C ================
 #@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 #@C file found in the top-level directory of this distribution and at
 #@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 #@C
 #@C No part of CPSW, including this file, may be copied, modified, propagated, or
 #@C distributed except according to the terms contained in the LICENSE.txt file.

from    libcpp.cast     cimport *
from    cython.operator cimport preincrement, dereference
from    libc.stdio      cimport fopen, fclose, FILE

cdef extern from "<memory>" namespace "std":
  cdef shared_ptr[T] static_pointer_cast[T,U](shared_ptr[U])
  cdef shared_ptr[T] dynamic_pointer_cast[T,U](shared_ptr[U])

cimport pycpsw
from    yaml_cpp cimport c_Node, Node

priv__ = object()

cdef class NoInit:
  def __cinit__(self, secret):
    if secret != priv__:
      raise ValueError("Cannot instantiate objects of this type directly")

cdef class Entry(NoInit):

  cdef cc_Entry ptr

  cpdef getName(self):
    return self.ptr.get().getName().decode('UTF-8','strict')

  cpdef getSize(self):
    return self.ptr.get().getSize()

  cpdef getDescription(self):
    return self.ptr.get().getDescription()

  cpdef getPollSecs(self):
    return self.ptr.get().getPollSecs()

  cpdef isHub(self):
    return Hub.make(self.ptr.get().isHub())

cdef class Child(Entry):

  cpdef getOwner(self):
    return Hub.make( dynamic_pointer_cast[CIChild, CIEntry](self.ptr).get().getOwner() )

  cpdef getNelms(self):
    return dynamic_pointer_cast[CIChild, CIEntry](self.ptr).get().getNelms()

  @staticmethod
  cdef make(cc_Child cp):
    po     = Child(priv__)
    po.ptr = static_pointer_cast[CIEntry, CIChild]( cp )
    return po

cdef class Hub(Entry):

  cpdef findByName(self, str path):
    return Path.make( dynamic_pointer_cast[CIHub, CIEntry](self.ptr).get().findByName( path.c_str() ) )

  cpdef getChild(self, name):
    return Child.make( dynamic_pointer_cast[CIHub, CIEntry](self.ptr).get().getChild( name ) )

  cpdef getChildren(self):
    cdef cc_Children children = dynamic_pointer_cast[CIHub, CIEntry](self.ptr).get().getChildren()
    cdef iterator          it = children.get().begin()
    cdef iterator         ite = children.get().end()
    rval = []
    while it != ite:
      rval.append( Child.make( dereference( it ) ) )
      preincrement( it )
    return rval

  @staticmethod
  cdef make(cc_Hub cp):
    po     = Hub(priv__)
    po.ptr = static_pointer_cast[CIEntry, CIHub]( cp )
    return po

cdef cppclass CPathVisitor(IPathVisitor):

  bool visitPre(self,  cc_ConstPath p):
    return self.visitPre( Path.makeConst( p ) )

  void visitPost(self, cc_ConstPath p):
    self.visitPost( Path.makeConst( p ) )

cdef cppclass CYamlFixup(IYamlFixup):
  void call(self, c_Node &c_root, c_Node &c_top):
    root = Node()
    top  = Node()
    root.c_node = c_root
    top.c_node  = c_top
    self.call( root, top )

cdef public class YamlFixup[type CpswPyWrapT_YamlFixup, object CpswPyWrapO_YamlFixup]:
  cdef CYamlFixup cc_YamlFixup

  def __cinit__(self):
    self.cc_YamlFixup = CYamlFixup( <PyObject*>self )

  def __call__(self, root, top):
    self.call(root, top)

cdef public class PathVisitor[type CpswPyWrapT_PathVisitor, object CpswPyWrapO_PathVisitor]:

  cdef CPathVisitor cc_PathVisitor

  def __cinit__(self):
    self.cc_PathVisitor = CPathVisitor( <PyObject*>self )

  cpdef bool visitPre(self, path):
    return True

  cpdef void visitPost(self, path):
    pass

cdef class Path(NoInit):
  cdef cc_ConstPath cptr
  cdef cc_Path      ptr

  cpdef findByName(self, path):
    bstr = path.encode('UTF-8')
    cdef const char *cpath = bstr;
    return Path.make( self.cptr.get().findByName( cpath ) )

  cpdef up(self):
    if not self.ptr:
      raise TypeError("Path is CONST")
    return Child.make( self.ptr.get().up() )

  cpdef empty(self):
    return self.cptr.get().empty()

  cpdef size(self):
    return self.cptr.get().size()

  cpdef clear(self, Hub h = None):
    if not self.ptr:
      raise TypeError("Path is CONST")
    if issubclass(type(h), Hub):
      self.ptr.get().clear( dynamic_pointer_cast[CIHub,CIEntry]( h.ptr ) )
    elif None == h:
      self.ptr.get().clear()
    else:
      raise TypeError("Expected a 'Hub' object here")

  cpdef origin(self):
    return Hub.make( self.cptr.get().origin() )

  cpdef parent(self):
    return Hub.make( self.cptr.get().parent() )

  cpdef tail(self):
    return Child.make( self.cptr.get().tail() )

  cpdef toString(self):
    return self.cptr.get().toString().decode('UTF-8','strict')

  def __repr__(self):
    return self.toString()

  cpdef dump(self, str fnam = None):
    cdef string cfnam
    cdef FILE *f

    if None == fnam:
      f = stdout
    else:
      cfnam = fnam.encode('UTF-8')
      f     = fopen( cfnam.c_str(), "w+" );
      if f == NULL:
        raise RuntimeError("Unable to open file: " + fnam);
    try:
      self.cptr.get().dump( f )
    finally:
      if f != stdout:
        fclose( f )

  cpdef verifyAtTail(self, Path path):
    # modifies 'this' path if it is empty
    if not self.ptr:
      raise TypeError("Path is CONST")
    return self.ptr.get().verifyAtTail( path.cptr )

  cpdef void append(self, Path path):
    if not self.ptr:
      raise TypeError("Path is CONST")
    self.ptr.get().append( path.cptr )

  cpdef concat(self, Path path):
    return Path.make( self.cptr.get().concat( path.cptr ) )

  cpdef clone(self):
    return Path.make( self.cptr.get().clone() )

  cpdef intersect(self, Path path):
    return Path.make( self.cptr.get().intersect( path.cptr ) )

  cpdef isIntersecting(self, Path path):
    return self.cptr.get().isIntersecting( path.cptr )

  cpdef getNelms(self):
    return self.cptr.get().getNelms()

  cpdef getTailFrom(self):
    return self.cptr.get().getTailFrom()

  cpdef getTailTo(self):
    return self.cptr.get().getTailTo()

  cpdef explore(self, PathVisitor visitor):
    if not issubclass(type(visitor), PathVisitor):
      raise TypeError("expected a PathVisitor argument")
    return self.cptr.get().explore( &visitor.cc_PathVisitor )

  cpdef loadConfigFromYamlFile(self, configYamlFileName, yamlIncDirName = None):
    cdef const char *cfnam
    cdef const char *cdnam = NULL
    fnam  = configYamlFileName.encode('UTF-8')
    cfnam = fnam
    dnam  = None
    if None != yamlIncDirName:
      dnam  = yamlIncDirName.encode('UTF-8')
      cdnam = dnam
    return wrap_Path_loadConfigFromYamlFile(self.ptr, cfnam, cdnam)

  cpdef loadConfigFromYamlString(self, configYamlString, yamlIncDirName = None):
    cdef const char *ccfgstr
    cdef const char *cdnam = NULL
    cfgstr  = configYamlString.encode('UTF-8')
    ccfgstr = cfgstr
    dnam  = None
    if None != yamlIncDirName:
      dnam  = yamlIncDirName.encode('UTF-8')
      cdnam = dnam
    return wrap_Path_loadConfigFromYamlString(self.ptr, ccfgstr, cdnam)

  cpdef dumpConfigToYamlFile(self, fileName, templFileName = None, yamlIncDirName = None):
    cdef const char *cfnam = NULL
    cdef const char *ctnam = NULL
    cdef const char *cydir = NULL
    fnam  = fileName.encode("UTF-8")
    cfnam = fnam
    tnam  = None
    bydir = None
    if None != templFileName:
      tnam  = templFileName.encode("UTF-8")
      ctnam = tnam
    if None != yamlIncDirName:
      bydir = yamlIncDirName.encode("UTF-8")
      cydir = bydir
    return wrap_Path_dumpConfigToYamlFile(self.ptr, cfnam, ctnam, cydir)

  cpdef dumpConfigToYamlString(self, templFileName = None, yamlIncDirName = None):
    cdef const char *ctnam = NULL
    cdef const char *cydir = NULL
    tnam  = None
    bydir = None
    if None != templFileName:
      tnam  = templFileName.encode("UTF-8")
      ctnam = tnam
    if None != yamlIncDirName:
      bydir = yamlIncDirName.encode("UTF-8")
      cydir = bydir
    return wrap_Path_dumpConfigToYamlString(self.ptr, ctnam, cydir)

  @staticmethod
  def create(arg = None):
    cdef const char *cpath
    if issubclass(type(arg), Hub):
      h = <Hub>arg
      return Path.make( IPath.create1( dynamic_pointer_cast[CIHub, CIEntry](h.ptr) ) )
    elif None == arg:
      return Path.make( IPath.create0() )
    elif issubclass(type(arg), str):
      bstr  = arg.encode('UTF-8')
      cpath = bstr;
      return Path.make( IPath.create2( cpath ) )
    else:
      raise TypeError("Expected a Hub object here")

  @staticmethod
  def loadYamlFile(str yamlFileName, str rootName="root", str yamlIncDirName = None, YamlFixup yamlFixup = None):
    cdef const char *cname = NULL
    cdef const char *croot = NULL
    cdef const char *cydir = NULL
    cdef IYamlFixup *cfixp = NULL;
    bname = yamlFileName.encode("UTF-8")
    cname = bname
    broot = rootName.encode("UTF-8")
    croot = broot
    bydir = None
    if None != yamlIncDirName:
      bydir = yamlIncDirName.encode("UTF-8")
      cydir = bydir

    if None != yamlFixup:
      cfixp = &yamlFixup.cc_YamlFixup

    return Path.make( IPath.loadYamlFile( cname, croot, cydir, cfixp ) )

  @staticmethod
  def loadYaml(str yamlString, str rootName="root", yamlIncDirName = None, YamlFixup yamlFixup = None):
    cdef string cyamlString = yamlString.encode("UTF-8")
    cdef const char *croot  = NULL
    cdef const char *cydir  = NULL
    cdef IYamlFixup *cfixp  = NULL;
    broot = rootName.encode("UTF-8")
    croot = broot
    bydir = None
    if None != yamlIncDirName:
      bydir = yamlIncDirName.encode("UTF-8")
      cydir = bydir

    if None != yamlFixup:
      cfixp = &yamlFixup.cc_YamlFixup

    return Path.make( wrap_Path_loadYamlStream( cyamlString, croot, cydir, cfixp ) )

  @staticmethod
  cdef make(cc_Path cp):
    po      = Path(priv__)
    po.ptr  = cp
    po.cptr = static_pointer_cast[CIPath, IPath]( cp )
    return po

  @staticmethod
  cdef makeConst(cc_ConstPath cp):
    po      = Path(priv__)
    po.cptr  = cp
    return po


cdef public class CPSWError(Exception)[type CpswPyExcT_CPSWError, object CpswPyExcO_CPSWError]:
  pass
cdef public class ErrnoError(CPSWError)[type CpswPyExcT_ErrnoError, object CpswPyExcO_ErrnoError]:
  pass
cdef public class IOError(ErrnoError)[type CpswPyExcT_IOError, object CpswPyExcO_IOError]:
  pass
cdef public class InternalError(ErrnoError)[type CpswPyExcT_InternalError, object CpswPyExcO_InternalError]:
  pass
cdef public class DuplicateNameError(CPSWError)[type CpswPyExcT_DuplicateNameError, object CpswPyExcO_DuplicateNameError]:
  pass
cdef public class NotDevError(CPSWError)[type CpswPyExcT_NotDevError, object CpswPyExcO_NotDevError]:
  pass
cdef public class NotFoundError(CPSWError)[type CpswPyExcT_NotFoundError, object CpswPyExcO_NotFoundError]:
  pass
cdef public class InvalidPathError(CPSWError)[type CpswPyExcT_InvalidPathError, object CpswPyExcO_InvalidPathError]:
  pass
cdef public class InvalidIdentError(CPSWError)[type CpswPyExcT_InvalidIdentError, object CpswPyExcO_InvalidIdentError]:
  pass
cdef public class InvalidArgError(CPSWError)[type CpswPyExcT_InvalidArgError, object CpswPyExcO_InvalidArgError]:
  pass
cdef public class AddressAlreadyAttachedError(CPSWError)[type CpswPyExcT_AddressAlreadyAttachedError, object CpswPyExcO_AddressAlreadyAttachedError]:
  pass
cdef public class ConfigurationError(CPSWError)[type CpswPyExcT_ConfigurationError, object CpswPyExcO_ConfigurationError]:
  pass
cdef public class AddrOutOfRangeError(CPSWError)[type CpswPyExcT_AddrOutOfRangeError, object CpswPyExcO_AddrOutOfRangeError]:
  pass
cdef public class ConversionError(CPSWError)[type CpswPyExcT_ConversionError, object CpswPyExcO_ConversionError]:
  pass
cdef public class InterfaceNotImplementedError(CPSWError)[type CpswPyExcT_InterfaceNotImplementedError, object CpswPyExcO_InterfaceNotImplementedError]:
  pass
cdef public class BadStatusError(CPSWError)[type CpswPyExcT_BadStatusError, object CpswPyExcO_BadStatusError]:
  pass
cdef public class IntrError(CPSWError)[type CpswPyExcT_IntrError, object CpswPyExcO_IntrError]:
  pass
cdef public class StreamDoneError(CPSWError)[type CpswPyExcT_StreamDoneError, object CpswPyExcO_StreamDoneError]:
  pass
cdef public class FailedStreamError(CPSWError)[type CpswPyExcT_FailedStreamError, object CpswPyExcO_FailedStreamError]:
  pass
cdef public class MissingOnceTagError(CPSWError)[type CpswPyExcT_MissingOnceTagError, object CpswPyExcO_MissingOnceTagError]:
  pass
cdef public class MissingIncludeFileNameError(CPSWError)[type CpswPyExcT_MissingIncludeFileNameError, object CpswPyExcO_MissingIncludeFileNameError]:
  pass
cdef public class NoYAMLSupportError(CPSWError)[type CpswPyExcT_NoYAMLSupportError, object CpswPyExcO_NoYAMLSupportError]:
  pass
cdef public class NoError(CPSWError)[type CpswPyExcT_NoError, object CpswPyExcO_NoError]:
  pass
cdef public class MultipleInstantiationError(CPSWError)[type CpswPyExcT_MultipleInstantiationError, object CpswPyExcO_MultipleInstantiationError]:
  pass
cdef public class BadSchemaVersionError(CPSWError)[type CpswPyExcT_BadSchemaVersionError, object CpswPyExcO_BadSchemaVersionError]:
  pass
cdef public class TimeoutError(CPSWError)[type CpswPyExcT_TimeoutError, object CpswPyExcO_TimeoutError]:
  pass
