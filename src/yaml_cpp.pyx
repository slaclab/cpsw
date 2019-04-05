# cython: c_string_type=str, c_string_encoding=ascii
# ^^^^^^ DO NOT REMOVE THE ABOVE LINE ^^^^^^^
#@C Copyright Notice
#@C ================
#@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
#@C file found in the top-level directory of this distribution and at
#@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
#@C
#@C No part of CPSW, including this file, may be copied, modified, propagated, or
#@C distributed except according to the terms contained in the LICENSE.txt file.

from    libcpp.string   cimport *
from    libcpp          cimport bool
from    cython.operator cimport dereference  as deref, preincrement as incr

from    enum            import Enum

ctypedef long long longlong;

cdef extern from "yaml_cpp_util.h":
  cdef const c_Node yamlNodeFind[K](c_Node &n, const K &k)       except+;
  cdef void yamlNodeSet[K,V](c_Node &n, const K &k, V &v)        except+;
  cdef void yamlNodeReset(c_Node *, const c_Node *)              except+;
  cdef void yamlNodeReset(c_Node *, const c_Node &)              except+;
  cdef bool boolNode(c_Node &)                                   except+;
  cdef string c_emitNode "emitNode"(const c_Node &)              except+;
  cdef int  handleIterator(c_Node *, c_Node *, const_iterator *) except+;
cdef int nodeTypeUndefined():
  return Undefined
cdef int nodeTypeNull():
  return Null
cdef int nodeTypeScalar():
  return Scalar
cdef int nodeTypeSequence():
  return Sequence
cdef int nodeTypeMap():
  return Map

class NodeType(Enum):
  Undefined = nodeTypeUndefined()
  Null      = nodeTypeNull()
  Scalar    = nodeTypeScalar()
  Sequence  = nodeTypeSequence()
  Map       = nodeTypeMap()

cdef class Node:

  def __cinit__(self, str s = None):
    cdef c_Node snod
    if None != s:
      snod = c_Node( s )
      yamlNodeReset( &self.c_node, &snod )

  def IsDefined(self):
    return self.c_node.IsDefined()

  def IsNull(self):
    return self.c_node.IsNull()

  def IsScalar(self):
    return self.c_node.IsScalar()

  def IsSequence(self):
    return self.c_node.IsSequence()

  def IsMap(self):
    return self.c_node.IsMap()

  def Scalar(self):
    return self.c_node.Scalar()

  def Type(self):
    return NodeType( self.c_node.Type() )

  def remove(self, key):
    cdef Node     knod
    cdef longlong kint
    cdef string   cstr
    if isinstance(key, Node):
      knod = key
      return self.c_node.remove( knod.c_node )
    elif isinstance(key, str):
      cstr = key
      return self.c_node.remove( cstr )
    elif isinstance(key, int):
      kint = key
      return self.c_node.remove( kint )
    else:
      raise TypeError('yaml_cpp::c_Node::remove not supported for argument type')

  def set(self, rhs):
    cdef Node   pnod
    cdef string bstr
    if isinstance(rhs, Node):
      pnod = rhs
      self.c_node = pnod.c_node
    elif isinstance(rhs, str):
      self.c_node = c_Node( rhs )
    else:
      raise TypeError('yaml_cpp::c_Node::set not supported for argument type')

  def push_back(self, Node rhs):
    self.c_node.push_back( rhs.c_node )

  def getAs(self):
    return self.c_node.as[string]()

  def __repr__(self):
    if isinstance(self, Node) and self.IsScalar():
      return self.c_node.as[string]()
    else:
      return object.__repr__(self)

  def __bool__(self):
    return boolNode( self.c_node )

  def __getitem__(self, key):
    cdef Node   knod
    cdef longlong kint
    cdef Node rval = Node()
    if isinstance(key, Node):
      knod = key
      yamlNodeReset( &rval.c_node, yamlNodeFind[c_Node](self.c_node, knod.c_node) )
    elif isinstance(key, str):
      yamlNodeReset( &rval.c_node, yamlNodeFind[string](self.c_node, key) )
    elif isinstance(key, int):
      kint = key
      yamlNodeReset( &rval.c_node, yamlNodeFind[longlong](self.c_node, kint) )
    else:
      raise TypeError("yaml_cpp::c_Node::__getitem__ unsupported key type")
    return rval

  def __setitem__(self, key, val):
    cdef Node   knod, vnod
    cdef longlong kint

    if isinstance(key, str):
      if isinstance(val, str):
        yamlNodeSet[string, string](self.c_node, key, val)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[string, c_Node  ](self.c_node, key, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    elif isinstance(key, int):
      kint = key
      if isinstance(val, str):
        yamlNodeSet[longlong, string](self.c_node, kint, val)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[longlong, c_Node  ](self.c_node, kint, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    elif isinstance(key, Node):
      knod = key
      if isinstance(val, str):
        yamlNodeSet[c_Node, string](self.c_node, knod.c_node, val)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[c_Node, c_Node  ](self.c_node, knod.c_node, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    else:
      raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported key type")

  def __iter__(self):
    return NodeIterator( self )

cdef class ValueType:
  cdef readonly Node first
  cdef readonly Node second
  def __cinit__(self, Node k, Node v):
    self.first  = k
    self.second = v

cdef class NodeIterator:

  def __cinit__(self, Node nod):
    yamlNodeReset( &self.c_node, &nod.c_node )
    self.c_it   = nod.c_node.begin()

  def __next__(self):
    if self.c_it == self.c_node.end():
      raise StopIteration()
    n1 = Node()
    n2 = Node()
    if handleIterator( &n1.c_node, &n2.c_node, &self.c_it ) == 1:
      incr( self.c_it )
      return n1
    else:
      incr( self.c_it )
      return ValueType(n1, n2)


def LoadFile(str filename):
  cdef Node   rval = Node()
  rval.c_node = c_LoadFile( filename )
  return rval

def emitNode(Node node):
  return c_emitNode( node.c_node )
