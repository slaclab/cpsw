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
  cdef cppclass const_iterator:
    value_type &operator++() const;
    value_type &operator*()  const;
    bool operator==(const_iterator &) const;
 
  cdef cppclass c_Node "YAML::Node":
    c_Node(const string&)             except+;
    c_Node()                          except+;
    bool IsDefined()                const;
    bool IsNull()                   const;
    bool IsScalar()                 const;
    bool IsSequence()               const;
    bool IsMap()                    const;
    value Type()          const;
    const string &Scalar()          except+;
    bool remove(const c_Node&)        except+;
    bool remove(const string&)      except+;
    bool remove(long long)          except+;
    c_Node &operator=(const c_Node &)   except+;
    c_Node &operator=(const string &) except+;
    c_Node &operator=(long long)      except+;
    void push_back(const c_Node &)    except+;
    string as[string]()             except+;
    bool operator!()                const;
    c_Node operator[](const c_Node&)    except+;
    c_Node operator[](const string&)  except+;
    c_Node operator[](long long)      except+;
    const_iterator begin()          const;
    const_iterator end()            const;

  cdef c_Node LoadFile(const string&) except+;

cdef extern from "yaml-cpp/yaml.h" namespace "YAML::iterator::":
  cdef cppclass value_type (c_Node):
    c_Node first;
    c_Node second;
 
cdef extern from "yaml_cpp_util.h":
  cdef const c_Node yamlNodeFind[K](c_Node &n, const K &k) except+;
  cdef void yamlNodeSet[K,V](c_Node &n, const K &k, V &v) except+;
  cdef bool boolNode(c_Node &) except+;
  cdef string c_emitNode "emitNode"(const c_Node &) except+;
