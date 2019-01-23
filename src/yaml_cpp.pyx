from    libcpp.string cimport *
from    libcpp        cimport bool
cimport yaml_cpp

ctypedef long long longlong;

cdef class PyNode:
  cdef Node c_node

  def __cinit__(self, s = None):
    if None == s:
      self.c_node = Node()
    else:
      bstr = s.encode('UTF-8') 
      self.c_node = Node( bstr )

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

  def remove(self, rhs):
    cdef PyNode pn
    cdef string bstr
    if isinstance(rhs, PyNode):
      pn = rhs
      return self.c_node.remove( pn.c_node )
    elif isinstance(rhs, str):
      bstr = rhs.encode('UTF-8') 
      return self.c_node.remove( bstr )
    else:
      raise TypeError('yaml_cpp::Node::remove not supported for argument type')

  def set(self, rhs):
    cdef PyNode pn
    cdef string bstr
    if isinstance(rhs, PyNode):
      pn = rhs
      self.c_node = pn.c_node
    elif isinstance(rhs, str):
      bstr = rhs.encode('UTF-8') 
      self.c_node = bstr
    else:
      raise TypeError('yaml_cpp::Node::set not supported for argument type')

  def push_back(self, PyNode rhs):
    self.c_node.push_back( rhs.c_node )

  def getAs(self):
    return self.c_node.as[string]().decode('UTF-8')

  def __repr__(self):
    return self.c_node.as[string]().decode('UTF-8')

  def __bool__(self):
    return boolNode( self.c_node )

  cdef PyNode getNode(PyNode self, PyNode key):
    cdef PyNode rval = PyNode()
    rval.c_node = yamlNodeFind[Node](self.c_node, key.c_node)
    return rval

  cdef PyNode getStr(PyNode self, string key):
    cdef PyNode rval = PyNode()
    rval.c_node = yamlNodeFind[string](self.c_node, key)
    return rval

  cdef PyNode getInt(PyNode self, longlong key):
    cdef PyNode rval = PyNode()
    rval.c_node = yamlNodeFind[longlong](self.c_node, key)
    return rval

  def __getitem__(self, key):
    cdef PyNode   knod
    cdef string   kstr
    cdef longlong kint
    cdef PyNode rval = PyNode()
    if isinstance(key, PyNode):
      knod = key
      rval.c_node = yamlNodeFind[Node](self.c_node, knod.c_node)
      return self.getNode( key )
    elif isinstance(key, str):
      kstr = key.encode('UTF-8')
      rval.c_node = yamlNodeFind[string](self.c_node, kstr)
    elif isinstance(key, int):
      kint = key
      rval.c_node = yamlNodeFind[longlong](self.c_node, kint)
    else:
      raise TypeError("yaml_cpp::Node::__getitem__ unsupported key type")
    return rval

  def __setitem__(self, key, val):
    cdef PyNode   knod, vnod
    cdef string   kstr, vstr
    cdef longlong kint

    if isinstance(key, str):
      kstr = key.encode('UTF-8')
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[string, string](self.c_node, kstr, vstr)
      elif isinstance(val, PyNode):
        vnod = val
        yamlNodeSet[string, Node  ](self.c_node, kstr, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::Node::__setitem__ unsupported value type")
    elif isinstance(key, int):
      kint = key
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[longlong, string](self.c_node, kint, vstr)
      elif isinstance(val, PyNode):
        vnod = val
        yamlNodeSet[longlong, Node  ](self.c_node, kint, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::Node::__setitem__ unsupported value type")
    elif isinstance(key, PyNode):
      knod = key
      if isinstance(val, str):
        vstr = val.encode('UTF-8')
        yamlNodeSet[Node, string](self.c_node, knod.c_node, vstr)
      elif isinstance(val, PyNode):
        vnod = val
        yamlNodeSet[Node, Node  ](self.c_node, knod.c_node, vnod.c_node)
      else:
        raise TypeError("yaml_cpp::Node::__setitem__ unsupported value type")
    else:
      raise TypeError("yaml_cpp::Node::__setitem__ unsupported key type")

  @staticmethod
  def LoadFile(string filename):
    cdef PyNode rval = PyNode()
    rval.c_node = LoadFile( filename )
    return rval

def emitNode(PyNode node):
  return c_emitNode( node.c_node )
