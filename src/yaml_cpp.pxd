 #@C Copyright Notice
 #@C ================
 #@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 #@C file found in the top-level directory of this distribution and at
 #@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 #@C
 #@C No part of CPSW, including this file, may be copied, modified, propagated, or
 #@C distributed except according to the terms contained in the LICENSE.txt file.

from libcpp.string cimport *
from libcpp        cimport bool

cdef extern from "yaml-cpp/yaml.h" namespace "YAML::NodeType":
  cdef enum value:
    Undefined
    Null
    Scalar
    Sequence
    Map

cdef extern from "yaml-cpp/yaml.h" namespace "YAML":

  # Forward Declaration
  cdef cppclass c_Node "YAML::Node"

  cdef cppclass const_iterator:
    value_type &operator++() const;
    value_type &operator*()  const;
    bool operator==(const_iterator &) const;
 
  cdef cppclass c_Node "YAML::Node":
    c_Node(const string&)             except+;
    c_Node()                          except+;
    bool IsDefined()                  except+;
    bool IsNull()                     except+;
    bool IsScalar()                   except+;
    bool IsSequence()                 except+;
    bool IsMap()                      except+;
    value Type()                      except+;
    const string &Scalar()            except+;
    bool remove(const c_Node&)        except+;
    bool remove(const string&)        except+;
    bool remove(long long)            except+;
    void reset(const c_Node& rhs = c_Node()) except+;
    void push_back(const c_Node &)    except+;
    string as[string]()               except+;
    bool operator!()                  except+;
    c_Node operator[](const c_Node&)  except+;
    c_Node operator[](const string&)  except+;
    c_Node operator[](long long)      except+;
    size_t size()                     except+;
    const_iterator begin()            except+;
    const_iterator end()              except+;

  cdef c_Node c_LoadFile "YAML::LoadFile"(const string&) except+;

cdef extern from "yaml-cpp/yaml.h" namespace "YAML::iterator::":
  cdef cppclass value_type (c_Node):
    c_Node first;
    c_Node second;
 
cdef class Node:
  cdef c_Node         c_node

cdef class NodeIterator:
  cdef const_iterator c_it
  cdef c_Node         c_node
