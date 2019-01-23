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
 
  cdef cppclass Node:
    Node(const string&)             except+;
    Node()                          except+;
    bool IsDefined()                const;
    bool IsNull()                   const;
    bool IsScalar()                 const;
    bool IsSequence()               const;
    bool IsMap()                    const;
    value Type()          const;
    const string &Scalar()          except+;
    bool remove(const Node&)        except+;
    bool remove(const string&)      except+;
    bool remove(long long)          except+;
    Node &operator=(const Node &)   except+;
    Node &operator=(const string &) except+;
    Node &operator=(long long)      except+;
    void push_back(const Node &)    except+;
    string as[string]()             except+;
    bool operator!()                const;
    Node operator[](const Node&)    except+;
    Node operator[](const string&)  except+;
    Node operator[](long long)      except+;
    const_iterator begin()          const;
    const_iterator end()            const;

  cdef Node LoadFile(const string&) except+;

cdef extern from "yaml-cpp/yaml.h" namespace "YAML::iterator::":
  cdef cppclass value_type (Node):
    Node first;
    Node second;
 
cdef extern from "yaml_cpp_util.h":
  cdef const Node yamlNodeFind[K](Node &n, const K &k) except+;
  cdef void yamlNodeSet[K,V](Node &n, const K &k, V &v) except+;
  cdef bool boolNode(Node &) except+;
  cdef string c_emitNode "emitNode"(const Node &) except+;
