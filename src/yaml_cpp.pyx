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
  cdef const c_Node yamlNodeFind[K](c_Node &n, const K &k) except+;
  cdef void yamlNodeSet[K,V](c_Node &n, const K &k, V &v) except+;
  cdef bool boolNode(c_Node &) except+;
  cdef string c_emitNode "emitNode"(const c_Node &) except+;
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
#  cdef c_Node         c_node
#  cdef const_iterator c_it

  def __cinit__(self, s = None):
    if None == s:
      self.c_node = c_Node()
    else:
      bstr = s.encode('UTF-8') 
      self.c_node = c_Node( bstr )

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
    return self.c_node.Scalar().decode('UTF-8')

  def Type(self):
    return NodeType( self.c_node.Type() )

  def remove(self, key):
    cdef Node   knod
    cdef string   kstr
    cdef longlong kint
    if isinstance(key, Node):
      knod = key
      return self.c_node.remove( knod.c_node )
    elif isinstance(key, str):
      kstr = key.encode('UTF-8') 
      return self.c_node.remove( kstr )
    elif isinstance(key, int):
      kint = key
      return self.c_node.remove( kint )
    else:
      raise TypeError('yaml_cpp::c_Node::remove not supported for argument type')

  def set(self, rhs):
    cdef Node pn
    cdef string bstr
    if isinstance(rhs, Node):
      pn = rhs
      self.c_node = pn.c_node
    elif isinstance(rhs, str):
      bstr = rhs.encode('UTF-8') 
      self.c_node = bstr
    else:
      raise TypeError('yaml_cpp::c_Node::set not supported for argument type')

  def push_back(self, Node rhs):
    self.c_node.push_back( rhs.c_node )

  def getAs(self):
    return self.c_node.as[string]().decode('UTF-8')

  def __repr__(self):
    if isinstance(self, Node) and self.IsScalar():
      return self.c_node.as[string]().decode('UTF-8')
    else:
      return object.__repr__(self)

  def __bool__(self):
    return boolNode( self.c_node )

  def __getitem__(self, key):
    cdef Node   knod
    cdef string   kstr
    cdef longlong kint
    cdef Node rval = Node()
    if isinstance(key, Node):
      knod = key
      rval.c_node = yamlNodeFind[c_Node](self.c_node, knod.c_node)
    elif isinstance(key, str):
      kstr = key.encode('UTF-8')
      rval.c_node = yamlNodeFind[string](self.c_node, kstr)
    elif isinstance(key, int):
      kint = key
      rval.c_node = yamlNodeFind[longlong](self.c_node, kint)
    else:
      raise TypeError("yaml_cpp::c_Node::__getitem__ unsupported key type")
    return rval

  def __setitem__(self, key, val):
    cdef Node   knod, vnod
    cdef string   kstr, vstr
    cdef longlong kint

    if isinstance(key, str):
      kstr = key.encode('UTF-8')
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[string, string](self.c_node, kstr, vstr)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[string, c_Node  ](self.c_node, kstr, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    elif isinstance(key, int):
      kint = key
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[longlong, string](self.c_node, kint, vstr)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[longlong, c_Node  ](self.c_node, kint, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    elif isinstance(key, Node):
      knod = key
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[c_Node, string](self.c_node, knod.c_node, vstr)
      elif isinstance(val, Node):
        vnod = val
        yamlNodeSet[c_Node, c_Node  ](self.c_node, knod.c_node, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported value type")
    else:
      raise TypeError("yaml_cpp::c_Node::__setitem__ unsupported key type")

  def __iter__(self):
    self.c_it = self.c_node.begin()
    return self

  def __next__(self):
    if self.c_it == self.c_node.end():
      raise StopIteration()
    if deref(self.c_it).IsDefined():
      x = Node()
      x.c_node = <c_Node> deref( self.c_it )
      rval = x
    else:
      k = Node()
      v = Node()
      k.c_node = deref( self.c_it ).first
      v.c_node = deref( self.c_it ).second
      rval = (k, v)
    incr( self.c_it );
    return rval

  @staticmethod
  def LoadFile(string filename):
    cdef Node rval = Node()
    rval.c_node = LoadFile( filename )
    return rval

def emitNode(Node node):
  return c_emitNode( node.c_node )
