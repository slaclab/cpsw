# cython: c_string_type=unicode, c_string_encoding=default
# ^^^^^^ DO NOT REMOVE THE ABOVE LINE ^^^^^^^
 #@C Copyright Notice
 #@C ================
 #@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 #@C file found in the top-level directory of this distribution and at
 #@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 #@C
 #@C No part of CPSW, including this file, may be copied, modified, propagated, or
 #@C distributed except according to the terms contained in the LICENSE.txt file.

from    cython.operator cimport preincrement, dereference
from    libc.stdio      cimport fopen, fclose, FILE
from    cpython.buffer  cimport PyObject_CheckBuffer
from    cpython.ref     cimport Py_XDECREF

cimport pycpsw
from    yaml_cpp cimport c_Node, Node

cdef extern from "<cpsw_shared_ptr.h>" namespace "cpsw":
  cdef shared_ptr[T] static_pointer_cast[T,U](shared_ptr[U])
  cdef shared_ptr[T] dynamic_pointer_cast[T,U](shared_ptr[U])

cdef extern from "cpsw_yaml.h" namespace "YAML":
  cdef cppclass ConvertEncoding "YAML::convert<IVal_Base::Encoding>":
    @staticmethod
    string do_encode(const ValEncoding &)

cdef extern from "ceval.h":
  cdef void PyEval_InitThreads()

priv__ = object()

PyEval_InitThreads()

cdef class NoInit:
  def __cinit__(self, *args):
    if args[0] != priv__:
      raise ValueError("Cannot instantiate objects of this type directly")

cdef class Entry(NoInit):
  """
Basic Node in the hierarchy
  """

  cdef cc_ConstEntry cptr

  def getName(self):
    """
Return the name of this Entry
    """
    return self.cptr.get().getName()

  def getSize(self):
    """
Return the size (in bytes) of the entity represented by this Entry
Note: if an array of Entries is present then 'getSize()' returns
      the size of each element. In case of a compound/container element
      the size of the entire compound is returned (including  arrays *in*
      the container -- but not covering arrays of the container itself).
    """
    return self.cptr.get().getSize()

  def getDescription(self):
    """
Return the description string (if any) of this Entry.
    """
    return self.cptr.get().getDescription()

  def getPollSecs(self):
    """
Return the suggested polling interval for this Entry.  
    """
    return self.cptr.get().getPollSecs()

  def dump(self, str fileName=None):
    """
Print information about this Entry to a file (stdout if 'None').
    """
    cdef FILE *f

    if None == fileName:
      f = stdout
    else:
      f = fopen( fileName, "w+" );
      if f == NULL:
        raise RuntimeError("Unable to open file: " + fileName);
    try:
      self.cptr.get().dump( f )
    finally:
      if f != stdout:
        fclose( f )


  def isHub(self):
    """
Test if this Entry is a Hub and return a reference.

If this Entry is *not* a Hub then 'None' is returned.
    """
    return Hub.make(self.cptr.get().isHub())

cdef class Child(Entry):
  """
An Entry which is attached to a Hub.
  """

  def getOwner(self):
    """
Return the Hub to which this Child is attached.
    """
    return Hub.make( dynamic_pointer_cast[CIChild, CIEntry](self.cptr).get().getOwner() )

  def getNelms(self):
    """
Return the number of elements this Child represents.

For array nodes the return value is > 1.
    """
    return dynamic_pointer_cast[CIChild, CIEntry](self.cptr).get().getNelms()

  @staticmethod
  cdef make(cc_ConstChild cp):
    if not cp:
      return None
    po      = Child(priv__)
    po.cptr = static_pointer_cast[CIEntry, CIChild]( cp )
    return po

cdef class Hub(Entry):
  """
Base class of all containers.
  """

  def findByName(self, str pathString):
    """
find all entries matching 'path' in or underneath this hub.
No wildcards are supported ATM. 'matching' refers to array
indices which may be used in the path.

E.g., if there is an array of four devices [0-3] then the
path may select just two of them:

     devices[1-2]

omitting explicit indices selects *all* instances of an array."

SEE ALSO: Path.findByName.

WARNING: do not use this method unless you know what you are
         doing. A Path which does not originate at the root
         cannot be used for communication and using it for
         instantiating interfaces (ScalVal, Command, ...) will
         result in I/O failures!
    """
    return Path.make( dynamic_pointer_cast[CIHub, CIEntry](self.cptr).get().findByName( pathString ) )

  def getChild(self, nameString):
    """
Return a child with name 'nameString' (or 'None' if no matching child exists).
    """
    return Child.make( dynamic_pointer_cast[CIHub, CIEntry](self.cptr).get().getChild( nameString ) )

  def getChildren(self):
    """
Return a list of all children.
    """
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
    if not cp:
      return None
    po      = Hub(priv__)
    po.cptr = static_pointer_cast[CIEntry, CIHub]( cp )
    return po

cdef class Val_Base(Entry):
  """
Base class for Val variants.
  """

  cdef cc_Val_Base ptr

  def getNelms(self):
    """
Return number of elements addressed by this ScalVal.

The Path used to instantiate a ScalVal may address an array
of scalar values. This method returns the number of array elements
    """
    return dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getNelms()

  def getPath(self):
    """
Return a copy of the Path which was used to create this ScalVal.
    """
    return Path.make( dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getPath() )

  def getConstPath(self):
    """
    """
    return Path.makeConst( dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getConstPath() )

  def getEncoding(self):
    """
Return a string which describes the encoding if this ScalVal is
itself e.g., a string. Common encodings include 'ASCII', 'UTF_8
Custom encodings are also communicated as 'CUSTOM_<integer>'.
If this Val has no defined encoding then the encoding should be <None>
(i.e., the object None). Note that "NONE" (string) is returned if
the encoding is explicitly set to "NONE" in the YAML definition.

The encoding may e.g., be used to convert a numerical array into
a string by python (second argument to the str() constructor).
The encoding 'IEEE_754' conveys that the ScalVal represents a
floating-point number.
    """
    cdef ValEncoding enc = dynamic_pointer_cast[CIVal_Base, CIEntry](self.cptr).get().getEncoding()
    if not enc:
      return None
    return ConvertEncoding.do_encode( enc )

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'Val_Base' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_Val_Base obj = IVal_Base.create( p.cptr )
    po      = Val_Base(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IVal_Base]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IVal_Base]( obj )
    return po

cdef class Enum(NoInit):
  """
An Enum object is a list of (string, numerical value) tuples.
  """
  cdef cc_Enum ptr

  def getItems(self):
    """
Return this enum converted into a python list.
We chose this over a dictionary (to which the list
can easily be converted) because it preserves the
original order of the menu entreis.
    """
    cdef EnumIterator it  = self.ptr.get().begin()
    cdef EnumIterator ite = self.ptr.get().end()
    cdef cc_ConstString cc_str
    rval = []
    while it != ite:
      cc_str = dereference( it ).first
      nam = cc_str.get().c_str()
      num = dereference( it ).second
      tup = ( nam, num )
      rval.append( tup )
      preincrement( it )
    return rval

  def getNelms(self):
    """
Return the number of entries in this Enum.
    """
    return self.ptr.get().getNelms()

cdef class ScalVal_Base(Val_Base):
  """
Base class for ScalVal variants.
  """

  def getSizeBits(self):
    """
Return the size in bits of this ScalVal.

If the ScalVal represents an array then the return value is the size
of each individual element.
    """
    return dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().getSizeBits()

  def isSigned(self):
    """
Return True if this ScalVal represents a signed number.

If the ScalVal is read into a wider number than its native bitSize
then automatic sign-extension is performed (for signed ScalVals).
    """
    return dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().isSigned()

  def getEnum(self):
    """
Return 'Enum' object associated with this ScalVal (if any).

An Enum object is a list of (str,int) tuples with associates strings
with numerical values.
    """
    cdef cc_Enum cenums = dynamic_pointer_cast[CIScalVal_Base, CIEntry](self.cptr).get().getEnum()

    if not cenums:
      return None

    enums     = Enum(priv__)
    enums.ptr = cenums
    return enums
    
  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'ScalVal_Base' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_ScalVal_Base obj = IScalVal_Base.create( p.cptr )
    po      = ScalVal_Base(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IScalVal_Base]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IScalVal_Base]( obj )
    return po

cdef class ScalVal_RO(ScalVal_Base):
  """
Read-Only interface for endpoints which support scalar values.

This interface supports reading integer values e.g., registers
or individual bits. It may also feature an associated map of
'enum strings'. E.g., a bit with such a map attached could be
read as 'True' or 'False'.

NOTE: If no write operations are required then it is preferable
      to use the ScalVal_RO interface (as opposed to ScalVal)
      since the underlying endpoint may be read-only.
  """

  # Cache a pointer for efficiency reasons (avoid dynamic_cast)
  cdef cc_ScalVal_RO rptr

  def getVal(self, *args, **kwargs):
    """
getVal(self, [bufferObject], fromIdx=-1, toIdx = -1, forceNumeric = False)

If no 'bufferObject is given:
=============================

Read one or multiple values, return as a scalar or a list.

If no indices (fromIdx, toIdx) are specified then all elements addressed by
the path object from which the ScalVal_RO was created are retrieved. All
values are represented by a 'c-style flattened' list:

  /some/dev[0-1]/item[0-9]

would return

  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].

The indices may be used to 'slice' the rightmost index in the path which
was used when creating the ScalVal interface. Enumerating these slices
starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx
**are based off the index contained in the path**!

E.g., if a ScalVal_RO created from the above path is read with:

  ScalVal_RO.create( root.findByName('/some/dev[0-1]/item[4-9]') )->getVal( 1, 2 )

then [ dev0_item5, dev0_item6, dev1_item5, dev1_item6 ] would be returned.
Note how 'fromIdx==1' is added to the starting index '4' from the path.
If both 'fromIdx' and 'toIdx' are negative then all elements are included.
A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only
the single element at 'fromIdx' to be read.

If the ScalVal_RO supports an 'enum' menu then the numerical values read from the
underlying hardware are mapped back to strings which are returned by 'getVal()'.
The optional 'forceNumeric' argument, when set to 'True', suppresses this
conversion and fetches the raw numerical values.

If a 'bufferObject is given:
============================

Read one or multiple values into a buffer, return the number of items read.

This variant of 'getVal()' reads values directly into 'bufObject' which must
support the ('new-style') buffer protocol (e.g., numpy.ndarray).
If the buffer is larger than the number of elements retrieved (i.e., the
return value of this method) then the excess elements are undefined.

The index limits ('fromIdx/toIdx') may be used to restrict the number of
elements read as described above.

No mapping to enum strings is supported by this variant of 'getVal()'.
    """
    # [buf], fromIdx = -1, toIdx = -1, forceNumeric = False):
    cdef int  fromIdx      = -1
    cdef int  toIdx        = -1
    cdef bool forceNumeric = False
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
        toIdx   = kwargs["toIdx"]

      if "forceNumeric" in kwargs:
        forceNumeric = kwargs["forceNumeric"]
  
    if i == 1: # read into buffer
      return IScalVal_RO_getVal( self.rptr.get(), <PyObject*>args[0], fromIdx, toIdx )

    po   = IScalVal_RO_getVal( self.rptr.get(), fromIdx, toIdx, forceNumeric)
    rval = <object>po # acquires a ref!
    Py_XDECREF( po )
    return rval
 
  def getValAsync(self, AsyncIO asyncIO, int fromIdx = -1, int toIdx = -1, bool forceNumeric = False):
    """
Provide an asynchronous callback which will be executed once data arrive
    """
    cdef cc_AsyncIO aio = asyncIO.c_AsyncIO.makeShared()
    return asyncIO.c_AsyncIO.issueGetVal( self.rptr.get(), fromIdx, toIdx, forceNumeric, aio );

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'ScalVal_RO' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_ScalVal_RO obj = IScalVal_RO.create( p.cptr )
    po      = ScalVal_RO(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IScalVal_RO]( obj )
    po.ptr  = static_pointer_cast[IVal_Base,IScalVal_RO]( obj )
    po.rptr = obj
    return po


cdef class ScalVal(ScalVal_RO):
  """
Interface for endpoints which support scalar values.

This interface supports reading/writing integer values e.g., registers
or individual bits. It may also feature an associated map of
'enum strings'. E.g., a bit with such a map attached could be
read/written as 'True' or 'False'.
  """

  cdef cc_ScalVal wptr

  def setVal(self, values, fromIdx=-1, toIdx=-1):
    """
If no indices (fromIdx, toIdx) are specified then all elements addressed by
the path object from which the ScalVal was created are written. All values
represented by a 'c-style flattened' list or array:

  /some/dev[0-1]/item[0-9]

would expect

  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].

The indices may be used to 'slice' the rightmost index in the path which
was used when creating the ScalVal interface. Enumerating these slices
starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx
**are based off the index contained in the path**!
E.g., if a ScalVal created from the above path is written with:

  ScalVal.create( root.findByName('/some/dev[0-1]/item[4-9]') )->setVal( [44,55], 1, 1 )

then dev[0]/item[5] would be written with 44 and dev[1]/item[5] with 55.
Note how 'fromIdx==1' is added to the starting index '4' from the path.
If both 'fromIdx' and 'toIdx' are negative then all elements are included.
A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only
the single element at 'fromIdx' to be written.

If the ScalVal has an associated enum 'menu' and 'values' are strings then
these strings are mapped by the enum to raw numerical values.

If the 'values' object implements the (new-style) buffer protocol then 'setVal()'
will extract values directly from the buffer. No enum strings are supported in
this case.
    """
    return IScalVal_setVal( self.wptr.get(), <PyObject*>values, fromIdx, toIdx )

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'ScalVal' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_ScalVal obj = IScalVal.create( p.cptr )
    po      = ScalVal(priv__)
    po.cptr = static_pointer_cast[CIEntry    , IScalVal]( obj )
    po.ptr  = static_pointer_cast[IVal_Base  , IScalVal]( obj )
    po.rptr = static_pointer_cast[IScalVal_RO, IScalVal]( obj )
    po.wptr = obj
    return po

cdef class DoubleVal_RO(Val_Base):
  """
Read-Only interface for endpoints which support 'double' values.

This interface is analogous to ScalVal_RO but reads from an
interface which supplies floating-point numbers.

NOTE: If no write operations are required then it is preferable
      to use the DoubleVal_RO interface (as opposed to DoubleVal)
      since the underlying endpoint may be read-only.
  """

  cdef cc_DoubleVal_RO rptr

  def getVal(self, fromIdx = -1, toIdx = -1):
    """
Read one or multiple values, return as a scalar or a list.

If no indices (fromIdx, toIdx) are specified then all elements addressed by
the path object from which the DoubleVal_RO was created are retrieved. All
values are represented by a 'c-style flattened' list:

  /some/dev[0-1]/item[0-9]

would return

  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].

The indices may be used to 'slice' the rightmost index in the path which
was used when creating the DoubleVal interface. Enumerating these slices
starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx
**are based off the index contained in the path**!
E.g., if a DoubleVal_RO created from the above path is read with:

  DoubleVal_RO.create( root.findByName('/some/dev[0-1]/item[4-9]') )->getVal( 1, 2 )

then [ dev0_item5, dev0_item6, dev1_item5, dev1_item6 ] would be returned.
Note how 'fromIdx==1' is added to the starting index '4' from the path.
If both 'fromIdx' and 'toIdx' are negative then all elements are included.
A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only
the single element at 'fromIdx' to be read.
    """
    po   = IDoubleVal_RO_getVal( self.rptr.get(), fromIdx, toIdx )
    rval = <object>po # acquires a ref!
    Py_XDECREF( po )
    return rval
 
  def getValAsync(self, AsyncIO asyncIO, int fromIdx = -1, int toIdx = -1):
    """
Provide an asynchronous callback which will be executed once data arrive.
    """
    cdef cc_AsyncIO aio = asyncIO.c_AsyncIO.makeShared()
    return asyncIO.c_AsyncIO.issueGetVal( self.rptr.get(), fromIdx, toIdx, aio );

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'DoubleVal_RO' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_DoubleVal_RO obj = IDoubleVal_RO.create( p.cptr )
    po      = DoubleVal_RO(priv__)
    po.cptr = static_pointer_cast[CIEntry,   IDoubleVal_RO]( obj )
    po.ptr  = static_pointer_cast[IVal_Base, IDoubleVal_RO]( obj )
    po.rptr = obj
    return po

cdef class DoubleVal(DoubleVal_RO):
  """
Interface for endpoints which support 'double' values.

This interface is analogous to ScalVal but accesses an
interface which expects/supplies floating-point numbers.
  """
  cdef cc_DoubleVal wptr


  def setVal(self, values, fromIdx=-1, toIdx=-1):
    """
Write one or multiple values, return the number of elements written.

If no indices (fromIdx, toIdx) are specified then all elements addressed by
the path object from which the DoubleVal was created are written. All values
represented by a 'c-style flattened' list or array:

  /some/dev[0-1]/item[0-9]

would expect

  [dev0_item0_value, dev0_item1_value, ..., dev0_item9_value, dev1_item0_value, ...].

The indices may be used to 'slice' the rightmost index in the path which
was used when creating the DoubleVal interface. Enumerating these slices
starts at 'fromIdx' up to and including 'toIdx'. Note that fromIdx/toIdx
**are based off the index contained in the path**!
E.g., if a DoubleVal created from the above path is written with:

  DoubleVal.create( root.findByName('/some/dev[0-1]/item[4-9]') )->setVal( [44,55], 1, 1 )

then dev[0]/item[5] would be written with 44 and dev[1]/item[5] with 55.
Note how 'fromIdx==1' is added to the starting index '4' from the path.
If both 'fromIdx' and 'toIdx' are negative then all elements are included.
A negative 'toIdx' is equivalent to 'toIdx' == 'fromIdx' and results in only
the single element at 'fromIdx' to be written.
    """
    return IDoubleVal_setVal( self.wptr.get(), <PyObject*>values, fromIdx, toIdx )

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'DoubleVal' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_DoubleVal obj = IDoubleVal.create( p.cptr )
    po      = DoubleVal(priv__)
    po.cptr = static_pointer_cast[CIEntry      , IDoubleVal]( obj )
    po.ptr  = static_pointer_cast[IVal_Base    , IDoubleVal]( obj )
    po.rptr = static_pointer_cast[IDoubleVal_RO, IDoubleVal]( obj )
    po.wptr = obj
    return po

cdef class Stream(Val_Base):
  """
Interface for endpoints with support streaming of raw data.
NOTE: A stream must be explicitly managed (i.e., destroyed/closed
      when nobody reads from it in order to avoid RSSI stalls).
      In python you must use a StreamMgr as a context manager
      and use the stream within a 'with' statement.
  """
  cdef cc_Stream sptr

  def read(self, bufObject, timeoutUs = -1, offset = 0):
    """
Read raw bytes from a streaming interface into a buffer and return the number of bytes read.

'bufObject' must support the (new-style) buffer protocol.

The 'timeoutUs' argument may be used to limit the time this
method blocks waiting for data to arrive. A (relative) timeout
in micro-seconds may be specified. A negative timeout blocks
indefinitely.
    """
    if not self.sptr: 
      raise FailedStreamError("Stream can only be read from the block of a 'with' statement!");
    return IStream_read( self.sptr.get(), <PyObject*>bufObject, timeoutUs, offset )

  def write(self, bufObject, timeoutUs = 0):
    """
Write raw bytes to a streaming interface from a buffer and return the number of bytes written.

'bufObject' must support the (new-style) buffer protocol.

The 'timeoutUs' argument may be used to limit the time this
method blocks waiting for data to be accepted. A (relative) timeout
in micro-seconds may be specified. A negative timeout blocks
indefinitely.
    """
    if not self.sptr: 
      raise FailedStreamError("Stream can only be written from the block of a 'with' statement!");
    return IStream_write( self.sptr.get(), <PyObject*>bufObject, timeoutUs )

  def __enter__(self):
    """
Open the underlying C++ Stream object for read/write access.
While open, the user *must* keep reading data -- otherwise
queues will fill up and eventually stall other TDEST channels
that are sharing a RSSI connection!
    """
    self.sptr = IStream.create( self.ptr.get().getPath() )
    return self

  def __exit__(self, exceptionType, exceptionValue, traceback):
    """
Close the underlying C++ Stream.
    """
    self.sptr.reset() # close
    return False;     # re-raise exception

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'Stream' context. Note that the Stream is opened/closed
by __enter__/__exit__. Reading/writing must be performed from a 'with'
statement.
    """
    # Just try to create the stream and immediately close it
    cdef cc_Stream   sobj = IStream.create( p.cptr )
    cdef cc_Val_Base vobj

    # Close the stream; only open during 'with' statement
    sobj.reset()
    vobj    = IVal_Base.create( p.cptr )
    po      = Stream(priv__)
    po.cptr = static_pointer_cast[CIEntry,  IVal_Base]( vobj )
    po.ptr  = static_pointer_cast[IVal_Base,IVal_Base]( vobj )
    return po

cdef class Command(Val_Base):
  """
The Command interface gives access to commands implemented by the underlying endpoint.

Details of the command are hidden. Execution runs the command or command sequence
coded by the endpoint.
  """

  cdef cc_Command cmdptr;

  def execute(self):
    """
Execute the command implemented by the endpoint addressed by the
path which was created when instantiating the Command interface.
    """
    self.cmdptr.get().execute()

  # Must use the 'p.cptr' (ConstPath) -- since we cannot rely on a non-const being passed!
  @staticmethod
  def create(Path p):
    """
Instantiate a 'Command' interface at the endpoint identified by 'path'

NOTE: an InterfaceNotImplementedError exception is thrown if the endpoint does
      not support this interface.
    """
    cdef cc_Command obj = ICommand.create( p.cptr )
    po         = Command(priv__)
    po.cptr    = static_pointer_cast[CIEntry,  ICommand]( obj )
    # We can't use the 'const IEntry' pointer because 'execute'
	# is not a 'const' method...
    po.cmdptr  = static_pointer_cast[ICommand ,ICommand]( obj )
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
    self.__call__( root, top )

cdef cppclass CAsyncIO(IAsyncIO):

  void callback(self, PyObject *result, PyObject *status):
    pyStatus = <object>status
    Py_XDECREF( status )
    pyResult = <object>result
    Py_XDECREF( result )
    self.callback( pyResult, pyStatus )

cdef public class YamlFixup[type CpswPyWrapT_YamlFixup, object CpswPyWrapO_YamlFixup]:
  """
The user must implement an implementation for this
interface which performs any desired fixup on the YAML
root node which is passed to the 'fixup' method

NOTE: you need python bindings for the yaml-cpp library
      in order to use this. Such bindings are NOT part
      of CPSW!

      void operator()(YAML::Node &root, YAML::Node &top)

      In python you must implement __call__(self, root, top)

  """
  cdef CYamlFixup cc_YamlFixup

  def __cinit__(self):
    self.cc_YamlFixup = CYamlFixup( <PyObject*>self )

  def __call__(self, root, top):
    """
Executed on the loaded YAML Node hierarchy.
    """
    self.__call__(root, top)

  @staticmethod
  def findByName(Node node, const char *path, char sep = b'/'):
    """
Lookup a YAML node from 'node' traversing a hierarchy
of YAML::Map's while handling merge keys ('<<').
The path to lookup is a string with path elements
separated by 'sep'
    """
    n = Node()
    n.c_node = wrap_IYamlFixup_findByName( node.c_node, path, sep )
    return n

cdef public class PathVisitor[type CpswPyWrapT_PathVisitor, object CpswPyWrapO_PathVisitor]:
  """
Traverse the hierarchy.


The user must implement an implementation for this
interface which performs any desired action on each
node in the hierarchy.

The 'IPath::explore()' method accepts an IPathVisitor
and recurses through the hierarchy calling into
IPathVisitor from each visited node.

The user must implement 'visitPre()' and 'visitPost()'
methods. Both methods are invoked for each node in the
hierarchy and a Path object leading to the node is supplied
to these methods. The user typically includes state information
in his/her implementation of the interface.

   bool visitPre (Path path)
   void visitPost(Path path)
  """

  cdef CPathVisitor cc_PathVisitor

  def __cinit__(self):
    self.cc_PathVisitor = CPathVisitor( <PyObject*>self )

  cpdef bool visitPre(self, path):
    """
visitPre() is executed before recursing into children of 'path'.
If the method returns 'False' then no recursion into children
occurs.
    """
    return True

  cpdef void visitPost(self, path):
    """
visitPost() is invoked after recursion into children and even
if recursion was skipped due to 'visitPre()' returning 'False'.
    """
    pass

cdef public class AsyncIO[type CpswPyWrapT_AsyncIO, object CpswPyWrapO_AsyncIO]:
  """
Callback base class for asynchronous 'getVal' operations. Subclass
must implement a

  callback(self, result, status)

member. This callback is passed the result of the asynchronous
operation and 'None' if there was an exception caught. In this
case the exception object is passed as 'status'.
  """

  cdef CAsyncIO c_AsyncIO

  def __cinit__(self):
    self.c_AsyncIO.init( <PyObject*>self )

cdef class Path(NoInit):
  """
Path objects are a 'handle' to a particular node in the hierarchy
they encode complete routing information from the 'tail' node up
to the root of the hierarchy. They also include index ranges for
array members that are in the path.

Paths are constructed by lookup and are passed to the factories
that instantiate accessor interfaces such as 'ScalVal', 'Command'
or 'Stream'.
  """
  cdef cc_ConstPath cptr
  cdef cc_Path      ptr

  def findByName(self, pathString):
    """
lookup 'pathString' starting from 'this' path and return a new Path object

'pathString' (a string object) may span multiple levels in the hierarchy,
separated by '/' - analogous to a file system.
However, CPSW supports the concept of 'array nodes', i.e., some nodes
(at any level of depth) may represent multiple identical devices or
registers. Array indices are zero-based.

When looking up an entity, the range(s) of items to address may be
restricted by specifying index ranges. E.g., if there are 4 identical
devices with ringbuffers of 32 elements each then a Path addressing
all of these could be specified:

      /prefix/device[0-3]/ringbuf[0-31]

If the user wishes only to access ringbuffer locations 8 to 12 in the
second device then they could construct a Path:

      /prefix/device[1]/ringbuf[8-12]

If a ScalVal is instantiated from this path then getVal/setVal would
only read/write the desired subset of registers

It should be noted that absence of a index specification is synonymous
to covering the full range. In the above example:

      /prefix/device/ringbuf

would be identical to

      /prefix/device[1]/ringbuf[8-12]

A 'NotFoundError' is thrown if the target of the operation does not exist.
    """
    return Path.make( self.cptr.get().findByName( pathString ) )

  def __add__(self, pathString):
    """
Shortcut for 'findByName'
    """
    return self.findByName( pathString )

  def up(self):
    """
Strip the last element off this path and return the child which
was at the tail prior to the operation.
    """
    if not self.ptr:
      raise TypeError("Path is CONST")
    return Child.make( self.ptr.get().up() )

  def empty(self):
    """
Test if this Path is empty returning 'True'/'False'.
    """
    return self.cptr.get().empty()

  def size(self):
    """
Return the depth of this Path, i.e., how many '/' separated
'levels' there are.
    """
    return self.cptr.get().size()

  def clear(self, Hub h = None):
    """
Clear this Path and reset starting at 'hub'.

Note: if 'hub' is not a true root device then future
communication attempts may fail due to lack of routing
information.
    """
    if not self.ptr:
      raise TypeError("Path is CONST")
    if issubclass(type(h), Hub):
      self.ptr.get().clear( dynamic_pointer_cast[CIHub,CIEntry]( h.cptr ) )
    elif None == h:
      self.ptr.get().clear()
    else:
      raise TypeError("Expected a 'Hub' object here")

  def origin(self):
    """
Return the Hub at the root of this path (if any -- 'None' otherwise).
    """
    cdef cc_ConstHub chub = self.cptr.get().origin()
    return Hub.make( chub )

  def parent(self):
    """
Return the parent Hub (if any -- 'None' otherwise).
    """
    cdef cc_ConstHub chub = self.cptr.get().parent()
    return Hub.make( chub )

  def tail(self):
    """
Return the child at the end of this Path (if any -- 'None' otherwise).
    """
    cdef cc_ConstChild cchild = self.cptr.get().tail()
    return Child.make( cchild )

  def toString(self):
    """
Convert this Path to a string representation.
    """
    return self.cptr.get().toString()

  def __repr__(self):
    return self.toString()

  def dump(self, str fileName = None):
    """
Print information about this Path to a file (stdout if 'None').
    """
    cdef FILE *f

    if None == fileName:
      f = stdout
    else:
      f = fopen( fileName, "w+" );
      if f == NULL:
        raise RuntimeError("Unable to open file: " + fileName);
    try:
      self.cptr.get().dump( f )
    finally:
      if f != stdout:
        fclose( f )

  def verifyAtTail(self, Path path):
    """
Verify that 'path' starts where 'this' Path ends; returns 'True'/'False'

If this condition does not hold then the two Paths cannot be concatenated
(and an exception would result).
    """
    # modifies 'this' path if it is empty
    if not self.ptr:
      raise TypeError("Path is CONST")
    return self.ptr.get().verifyAtTail( path.cptr )

  def append(self, Path path):
    """
Append a copy of 'path' to 'this' one.

If verifyAtTail(path) returns 'False' then 'append' raises an 'InvalidPathError'.
    """
    if not self.ptr:
      raise TypeError("Path is CONST")
    self.ptr.get().append( path.cptr )

  def concat(self, Path path):
    """
Append a copy of 'path' to 'this' one and return a copy of the result.

If verifyAtTail(path) returns 'False' then 'append' raises an 'InvalidPathError'.
Note: 'append()' modifies the original path whereas 'concat' returns
      a new copy.
    """
    return Path.make( self.cptr.get().concat( path.cptr ) )

  def clone(self):
    """
Return a copy of this Path.
    """
    return Path.make( self.cptr.get().clone() )

  def intersect(self, Path path):
    """
Return a new Path which covers the intersection of this path and 'path'

The new path contains all array elements common to both paths. Returns
an empty path if there are no common elements or if the depth of the
paths differs.
    """
    cdef cc_Path cpath = self.cptr.get().intersect( path.cptr )
    return Path.make( cpath )

  def isIntersecting(self, Path path):
    """
A slightly more efficient version of 'intersect'

If you only want to know if the intersection is (not)empty then isIntersecting
is more efficent.
    """
    return self.cptr.get().isIntersecting( path.cptr )

  def getNelms(self):
    """
Count and return the number of array elements addressed by this path.

This sums up array elements at *all* levels. E.g., executed on a Path
which represents 'device[0-3]/reg[0-3]' 'getNelms()' would yield 16.
    """
    return self.cptr.get().getNelms()

  def getTailFrom(self):
    """
Return the 'to' index addressed by the tail of this path.

E.g., executed on a Path which represents 'device[0-3]/reg[1-2]'
the method would yield 2. (The tail element is 'reg' and the 'to'
index is 2.)
    """
    return self.cptr.get().getTailFrom()

  def getTailTo(self):
    """
Return the 'to' index addressed by the tail of this path.

E.g., executed on a Path which represents 'device[0-3]/reg[1-2]'
the method would yield 2. (The tail element is 'reg' and the 'to'
index is 2.)
    """
    return self.cptr.get().getTailTo()

  def explore(self, PathVisitor pathVisitor):
    """
Recurse through the hierarchy rooted at 'this' Path and apply 'pathVisitor' to every descendent.

See 'PathVisitor' for more information.
    """
    if not issubclass(type(pathVisitor), PathVisitor):
      raise TypeError("expected a PathVisitor argument")
    return self.cptr.get().explore( &pathVisitor.cc_PathVisitor )

  def loadConfigFromYamlFile(self, configYamlFileName, yamlIncDirName = None):
    """
Load a configuration file in YAML format and write out into the hardware.

'yamlIncDirname' may point to a directory where included YAML files can
be found. Defaults to the directory where the YAML file is located.\n
RETURNS: number of values successfully written out.
    """
    cdef const char *cdnam = NULL
    if None != yamlIncDirName:
      cdnam  = yamlIncDirName
    return wrap_Path_loadConfigFromYamlFile(self.cptr, configYamlFileName, cdnam)

  def loadConfigFromYamlString(self, configYamlString, yamlIncDirName = None):
    """
Load a configuration from a YAML formatted string and write out into the hardware.

'yamlIncDirname' may point to a directory where included YAML files can be found.
Defaults to the directory where the YAML file is located.\n
RETURNS: number of values successfully written out.
    """
    cdef const char *cdnam = NULL
    if None != yamlIncDirName:
      cdnam = yamlIncDirName
    return wrap_Path_loadConfigFromYamlString(self.cptr, configYamlString, cdnam)

  def dumpConfigToYamlFile(self, fileName, templFileName = None, yamlIncDirName = None):
    """
Read a configuration from hardware and save to a file in YAML format.
If 'templFileName' is given then the paths listed in there (and only those)
are used in the order defined in 'templFileName'. Otherwise, the 'configPrio'
property of each field in the hierarchy is honored (see README.configData).
'yamlIncDirname' can be used to specify where additional YAML files are
stored.\n
RETURNS: number of values successfully saved to file.
    """
    cdef const char *ctnam = NULL
    cdef const char *cydir = NULL
    if None != templFileName:
      ctnam  = templFileName
    if None != yamlIncDirName:
      cydir = yamlIncDirName
    return wrap_Path_dumpConfigToYamlFile(self.cptr, fileName, ctnam, cydir)

  def dumpConfigToYamlString(self, template = None, yamlIncDirName = None, templateIsFilename = True):
    """
Read a configuration from hardware and return as a string in YAML format.
If 'template' is given then the CPSW paths listed in there (and only those)
are used in the order defined in the 'template'. Otherwise, the 'configPrio'
property of each field in the hierarchy is honored (see README.configData).
'yamlIncDirname' can be used to specify where additional YAML files are
stored.

If 'templateIsFilename' is True then 'template' is the filename of a template
file. Otherwise 'template' is a string containing the template itself.
    """
    cdef const char *ctmpl = NULL
    cdef const char *cydir = NULL
    if None != template:
      ctmpl = template
    if None != yamlIncDirName:
      cydir = yamlIncDirName
    return wrap_Path_dumpConfigToYamlString(self.cptr, ctmpl, cydir, templateIsFilename)

  @staticmethod
  def create(arg = None):
    """
Create a new Path originating at 'hub'

Note: if 'hub' is not a true root device then future
communication attempts may fail due to lack of routing
information.
    """
    cdef const char *cpath
    if issubclass(type(arg), Hub):
      h = <Hub>arg
      return Path.make( IPath.create1( dynamic_pointer_cast[CIHub, CIEntry](h.cptr) ) )
    elif None == arg:
      return Path.make( IPath.create0() )
    elif issubclass(type(arg), str):
      return Path.make( IPath.create2( arg ) )
    else:
      raise TypeError("Expected a Hub object here")

  @staticmethod
  def loadYamlFile(str yamlFileName, str rootName="root", str yamlIncDirName = None, YamlFixup yamlFixup = None):
    """
Load a hierarchy definition in YAML format from a file.

The hierarchy is built from the node with name 'rootName' (defaults to 'root').

Optionally, 'yamlIncDirName' may be passed which identifies a directory
where *all* yaml files reside. 'None' (or empty) instructs the method to
use the same directory where 'fileName' resides.

The directory is relevant for included YAML files.

RETURNS: Root Path of the device hierarchy.
    """
    cdef const char *cydir = NULL
    cdef IYamlFixup *cfixp = NULL;
    if None != yamlIncDirName:
      cydir = yamlIncDirName

    if None != yamlFixup:
      cfixp = &yamlFixup.cc_YamlFixup

    return Path.make( IPath.loadYamlFile( yamlFileName, rootName, cydir, cfixp ) )

  @staticmethod
  def loadYaml(str yamlString, str rootName="root", yamlIncDirName = None, YamlFixup yamlFixup = None):
    """
Load a hierarchy definition in YAML format from a string.

The hierarchy is built from the node with name 'rootName' (defaults to 'root').

Optionally, 'yamlIncDirName' may be passed which identifies a directory
where *all* yaml files reside. 'None' (or empty) instructs the method to
use the current working directory.

The directory is relevant for included YAML files.

RETURNS: Root Path of the device hierarchy.
    """
    cdef const char *cydir  = NULL
    cdef IYamlFixup *cfixp  = NULL;
    if None != yamlIncDirName:
      cydir = yamlIncDirName

    if None != yamlFixup:
      cfixp = &yamlFixup.cc_YamlFixup

    return Path.make( wrap_Path_loadYamlStream( yamlString, rootName, cydir, cfixp ) )

  @staticmethod
  cdef make(cc_Path cp):
    if not cp:
      return None
    po      = Path(priv__)
    po.ptr  = cp
    po.cptr = static_pointer_cast[CIPath, IPath]( cp )
    return po

  @staticmethod
  cdef makeConst(cc_ConstPath cp):
    if not cp:
      return None
    po      = Path(priv__)
    po.cptr  = cp
    return po

def getCPSWVersionString():
  return c_getCPSWVersionString()

def setCPSWVerbosity(str facility = None, int level = 0):
  """
Set verbosity level for debugging messages of different
Facilities. Invocation w/o arguments prints a list of
currently supported facilities.
  """

  cdef const char *cstr = NULL;
  if None != facility:
    cstr = facility
  return c_setCPSWVerbosity( cstr, level )

cdef public class CPSWError(Exception)[type CpswPyExcT_CPSWError, object CpswPyExcO_CPSWError]:

  def __init__(self, str msg):
    self._msg = msg

  def what(self):
    return self._msg

cdef public class ErrnoError(CPSWError)[type CpswPyExcT_ErrnoError, object CpswPyExcO_ErrnoError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class IOError(ErrnoError)[type CpswPyExcT_IOError, object CpswPyExcO_IOError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class InternalError(ErrnoError)[type CpswPyExcT_InternalError, object CpswPyExcO_InternalError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class DuplicateNameError(CPSWError)[type CpswPyExcT_DuplicateNameError, object CpswPyExcO_DuplicateNameError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class NotDevError(CPSWError)[type CpswPyExcT_NotDevError, object CpswPyExcO_NotDevError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class NotFoundError(CPSWError)[type CpswPyExcT_NotFoundError, object CpswPyExcO_NotFoundError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class InvalidPathError(CPSWError)[type CpswPyExcT_InvalidPathError, object CpswPyExcO_InvalidPathError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class InvalidIdentError(CPSWError)[type CpswPyExcT_InvalidIdentError, object CpswPyExcO_InvalidIdentError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class InvalidArgError(CPSWError)[type CpswPyExcT_InvalidArgError, object CpswPyExcO_InvalidArgError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class AddressAlreadyAttachedError(CPSWError)[type CpswPyExcT_AddressAlreadyAttachedError, object CpswPyExcO_AddressAlreadyAttachedError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class ConfigurationError(CPSWError)[type CpswPyExcT_ConfigurationError, object CpswPyExcO_ConfigurationError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class AddrOutOfRangeError(CPSWError)[type CpswPyExcT_AddrOutOfRangeError, object CpswPyExcO_AddrOutOfRangeError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class ConversionError(CPSWError)[type CpswPyExcT_ConversionError, object CpswPyExcO_ConversionError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class InterfaceNotImplementedError(CPSWError)[type CpswPyExcT_InterfaceNotImplementedError, object CpswPyExcO_InterfaceNotImplementedError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class BadStatusError(CPSWError)[type CpswPyExcT_BadStatusError, object CpswPyExcO_BadStatusError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class IntrError(CPSWError)[type CpswPyExcT_IntrError, object CpswPyExcO_IntrError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class StreamDoneError(CPSWError)[type CpswPyExcT_StreamDoneError, object CpswPyExcO_StreamDoneError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class FailedStreamError(CPSWError)[type CpswPyExcT_FailedStreamError, object CpswPyExcO_FailedStreamError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class MissingOnceTagError(CPSWError)[type CpswPyExcT_MissingOnceTagError, object CpswPyExcO_MissingOnceTagError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class MissingIncludeFileNameError(CPSWError)[type CpswPyExcT_MissingIncludeFileNameError, object CpswPyExcO_MissingIncludeFileNameError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class NoYAMLSupportError(CPSWError)[type CpswPyExcT_NoYAMLSupportError, object CpswPyExcO_NoYAMLSupportError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class NoError(CPSWError)[type CpswPyExcT_NoError, object CpswPyExcO_NoError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class MultipleInstantiationError(CPSWError)[type CpswPyExcT_MultipleInstantiationError, object CpswPyExcO_MultipleInstantiationError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class BadSchemaVersionError(CPSWError)[type CpswPyExcT_BadSchemaVersionError, object CpswPyExcO_BadSchemaVersionError]:
  def __init__(self, str msg):
    self._msg = msg

cdef public class TimeoutError(CPSWError)[type CpswPyExcT_TimeoutError, object CpswPyExcO_TimeoutError]:
  def __init__(self, str msg):
    self._msg = msg
