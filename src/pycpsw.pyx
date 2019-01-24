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
from    cpython.buffer  cimport PyObject_CheckBuffer
from    cpython.ref     cimport Py_XDECREF

cdef extern from "<memory>" namespace "std":
  cdef shared_ptr[T] static_pointer_cast[T,U](shared_ptr[U])
  cdef shared_ptr[T] dynamic_pointer_cast[T,U](shared_ptr[U])

cimport pycpsw
from    yaml_cpp cimport c_Node, Node

cdef extern from "cpsw_yaml.h" namespace "YAML":
  cdef cppclass ConvertEncoding "YAML::convert<IVal_Base::Encoding>":
    @staticmethod
    string do_encode(const ValEncoding &)

priv__ = object()

cdef class NoInit:
  def __cinit__(self, *args):
    if args[0] != priv__:
      raise ValueError("Cannot instantiate objects of this type directly")

cdef class Entry(NoInit):

  cdef cc_ConstEntry cptr

  def getName(self):
    return self.cptr.get().getName().decode('UTF-8','strict')

  def getSize(self):
    return self.cptr.get().getSize()

  def getDescription(self):
    return self.cptr.get().getDescription()

  def getPollSecs(self):
    return self.cptr.get().getPollSecs()

  def isHub(self):
    return Hub.make(self.cptr.get().isHub())

cdef class Child(Entry):

  def getOwner(self):
    return Hub.make( dynamic_pointer_cast[CIChild, CIEntry](self.cptr).get().getOwner() )

  def getNelms(self):
    return dynamic_pointer_cast[CIChild, CIEntry](self.cptr).get().getNelms()

  @staticmethod
  cdef make(cc_ConstChild cp):
    po      = Child(priv__)
    po.cptr = static_pointer_cast[CIEntry, CIChild]( cp )
    return po

cdef class Hub(Entry):

  def findByName(self, str path):
    return Path.make( dynamic_pointer_cast[CIHub, CIEntry](self.cptr).get().findByName( path.c_str() ) )

  def getChild(self, name):
    return Child.make( dynamic_pointer_cast[CIHub, CIEntry](self.cptr).get().getChild( name ) )

  def getChildren(self):
    cdef cc_ConstChildren children = dynamic_pointer_cast[CIHub, CIEntry](self.cptr).get().getChildren()
    cdef iterator          it = children.get().begin()
    cdef iterator         ite = children.get().end()
    rval = []
    while it != ite:
      rval.append( Child.make( dereference( it ) ) )
      preincrement( it )
    return rval

  @staticmethod
  cdef make(cc_ConstHub cp):
    po      = Hub(priv__)
    po.cptr = static_pointer_cast[CIEntry, CIHub]( cp )
    return po

cdef class Val_Base(Entry):

  cdef cc_Val_Base ptr

  def getNelms(self):
    return dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getNelms()

  def getPath(self):
    return Path.make( dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getPath() )

  def getConstPath(self):
    return Path.makeConst( dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getConstPath() )

  def getEncoding(self):
    cdef ValEncoding enc = dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getEncoding()
    return ConvertEncoding.do_encode( enc ).decode('UTF-8', 'strict')

  @staticmethod
  def create(Path p):
    cdef cc_Val_Base obj = IVal_Base.create( p.ptr )
    po      = Val_Base(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IVal_Base]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IVal_Base]( obj )
    return po

cdef class Enum(NoInit):
  cdef cc_Enum ptr

  def getItems(self):
    cdef EnumIterator it  = self.ptr.get().begin()
    cdef EnumIterator ite = self.ptr.get().end()
    cdef cc_ConstString cc_str
    rval = []
    while it != ite:
      cc_str = dereference( it ).first
      nam = cc_str.get().c_str().decode('UTF-8', 'strict')
      num = dereference( it ).second
      tup = ( nam, num )
      rval.append( tup )
      preincrement( it )
    return rval

  def getNelms(self):
    return self.ptr.get().getNelms()

cdef class ScalVal_Base(Val_Base):

  def getSizeBits(self):
    return dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().getSizeBits()

  def isSigned(self):
    return dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().isSigned()

  def getEnum(self):
    cdef cc_Enum cenums = dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().getEnum()

    enums = Enum(priv__)
    enums.ptr = cenums
    return enums
    

  @staticmethod
  def create(Path p):
    cdef cc_ScalVal_Base obj = IScalVal_Base.create( p.ptr )
    po      = ScalVal_Base(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IScalVal_Base]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IScalVal_Base]( obj )
    return po

cdef class ScalVal_RO(ScalVal_Base):

  def getVal(self, *args, **kwargs):
    # [buf], fromIdx = -1, toIdx = -1, forceNumeric = False):
    cdef int  fromIdx      = -1
    cdef int  toIdx        = -1
    cdef bool forceNumeric = False
    cdef cc_ScalVal_RO c_ptr
    cdef PyObject *po
    l = len(args)
    i = 0
    if l > 0:
      if PyObject_CheckBuffer( args[0] ):
        i = 1
      if i+0 < l:
        fromIdx = args[i]
        if i+1 < l:
          toIdx   = args[i]
          if i+2 < l:
            forceNumeric = args[i]

    if len(kwargs) > 0:
      if "fromIdx" in kwargs:
        fromIdx = kwargs["fromIdx"]

      if "toIdx" in kwargs:
        fromIdx = kwargs["toIdx"]

      if "forceNumeric" in kwargs:
        forceNumeric = kwargs["forceNumeric"]
  
    c_ptr = dynamic_pointer_cast[IScalVal_RO, IVal_Base]( self.ptr )

    if i == 1: # read into buffer
      return IScalVal_RO_getVal( c_ptr.get(), <PyObject*>args[0], fromIdx, toIdx )

    po   = IScalVal_RO_getVal( c_ptr.get(), fromIdx, toIdx, forceNumeric)
    rval = <object>po # acquires a ref!
    Py_XDECREF( po )
    return rval
 
  def getValAsync(self, aio, fromIdx = -1, toIdx = -1, forceNumeric = False):
    pass

cdef class ScalVal(ScalVal_RO):

  def setVal(self, values, fromIdx=-1, toIdx=-1):
    cdef cc_ScalVal c_ptr
    c_ptr = dynamic_pointer_cast[IScalVal, IVal_Base]( self.ptr )
    return IScalVal_setVal( c_ptr.get(), <PyObject*>values, fromIdx, toIdx )

  @staticmethod
  def create(Path p):
    cdef cc_ScalVal obj = IScalVal.create( p.ptr )
    po      = ScalVal(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IScalVal]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IScalVal]( obj )
    return po

cdef class Stream(ScalVal_Base):
  cdef cc_Stream sptr

  def read(self, bufObject, timeoutUs = -1, offset = 0):
    if not self.sptr: 
      raise FailedStreamError("Stream can only be read from the block of a 'with' statement!");
    return IStream_read( self.sptr.get(), <PyObject*>bufObject, timeoutUs, offset )

  def write(self, bufObject, timeoutUs = 0):
    if not self.sptr: 
      raise FailedStreamError("Stream can only be written from the block of a 'with' statement!");
    return IStream_write( self.sptr.get(), <PyObject*>bufObject, timeoutUs )

  def __enter__(self):
    self.sptr = IStream.create( self.ptr.get().getPath() )
    return self

  def __exit__(self, exceptionType, exceptionValue, traceback):
    self.sptr.reset() # close
    return False;     # re-raise exception

  @staticmethod
  def create(Path p):
    # Just try to create the stream and immediately close it
    cdef cc_Stream   sobj = IStream.create( p.ptr )
    cdef cc_Val_Base vobj

    # Close the stream; only open during 'with' statement
    sobj.reset()
    vobj    = IVal_Base.create( p.ptr )
    po      = Stream(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IVal_Base]( vobj )
    po.ptr  = static_pointer_cast[IVal_Base,IVal_Base]( vobj )
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

  def findByName(self, path):
    bstr = path.encode('UTF-8')
    cdef const char *cpath = bstr;
    return Path.make( self.cptr.get().findByName( cpath ) )

  def up(self):
    if not self.ptr:
      raise TypeError("Path is CONST")
    return Child.make( self.ptr.get().up() )

  def empty(self):
    return self.cptr.get().empty()

  def size(self):
    return self.cptr.get().size()

  def clear(self, Hub h = None):
    if not self.ptr:
      raise TypeError("Path is CONST")
    if issubclass(type(h), Hub):
      self.ptr.get().clear( dynamic_pointer_cast[CIHub,CIEntry]( h.cptr ) )
    elif None == h:
      self.ptr.get().clear()
    else:
      raise TypeError("Expected a 'Hub' object here")

  def origin(self):
    return Hub.make( self.cptr.get().origin() )

  def parent(self):
    return Hub.make( self.cptr.get().parent() )

  def tail(self):
    return Child.make( self.cptr.get().tail() )

  def toString(self):
    return self.cptr.get().toString().decode('UTF-8','strict')

  def __repr__(self):
    return self.toString()

  def dump(self, str fnam = None):
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

  def verifyAtTail(self, Path path):
    # modifies 'this' path if it is empty
    if not self.ptr:
      raise TypeError("Path is CONST")
    return self.ptr.get().verifyAtTail( path.cptr )

  def append(self, Path path):
    if not self.ptr:
      raise TypeError("Path is CONST")
    self.ptr.get().append( path.cptr )

  def concat(self, Path path):
    return Path.make( self.cptr.get().concat( path.cptr ) )

  def clone(self):
    return Path.make( self.cptr.get().clone() )

  def intersect(self, Path path):
    return Path.make( self.cptr.get().intersect( path.cptr ) )

  def isIntersecting(self, Path path):
    return self.cptr.get().isIntersecting( path.cptr )

  def getNelms(self):
    return self.cptr.get().getNelms()

  def getTailFrom(self):
    return self.cptr.get().getTailFrom()

  def getTailTo(self):
    return self.cptr.get().getTailTo()

  def explore(self, PathVisitor visitor):
    if not issubclass(type(visitor), PathVisitor):
      raise TypeError("expected a PathVisitor argument")
    return self.cptr.get().explore( &visitor.cc_PathVisitor )

  def loadConfigFromYamlFile(self, configYamlFileName, yamlIncDirName = None):
    cdef const char *cfnam
    cdef const char *cdnam = NULL
    fnam  = configYamlFileName.encode('UTF-8')
    cfnam = fnam
    dnam  = None
    if None != yamlIncDirName:
      dnam  = yamlIncDirName.encode('UTF-8')
      cdnam = dnam
    return wrap_Path_loadConfigFromYamlFile(self.ptr, cfnam, cdnam)

  def loadConfigFromYamlString(self, configYamlString, yamlIncDirName = None):
    cdef const char *ccfgstr
    cdef const char *cdnam = NULL
    cfgstr  = configYamlString.encode('UTF-8')
    ccfgstr = cfgstr
    dnam  = None
    if None != yamlIncDirName:
      dnam  = yamlIncDirName.encode('UTF-8')
      cdnam = dnam
    return wrap_Path_loadConfigFromYamlString(self.ptr, ccfgstr, cdnam)

  def dumpConfigToYamlFile(self, fileName, templFileName = None, yamlIncDirName = None):
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

  def dumpConfigToYamlString(self, templFileName = None, yamlIncDirName = None):
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
      return Path.make( IPath.create1( dynamic_pointer_cast[CIHub, CIEntry](h.cptr) ) )
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
