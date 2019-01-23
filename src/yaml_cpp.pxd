from libcpp.string cimport *
from libcpp        cimport bool

cdef extern from "yaml-cpp/yaml.h" namespace "YAML":
  cdef cppclass Node:
    Node(const string&)             except+;
    Node()                          except+;
    bool IsDefined()                const;
    bool IsNull()                   const;
    bool IsScalar()                 const;
    bool IsSequence()               const;
    bool IsMap()                    const;
    const string &Scalar()          except+;
    bool remove(const Node&)        except+;
    bool remove(const string&)      except+;
    Node &operator=(const Node &)   except+;
    Node &operator=(const string &) except+;
    Node &operator=(long long)      except+;
    void push_back(const Node &)    except+;
    string as[string]()             except+;
    bool operator!()                const;
    Node operator[](const Node&)    except+;
    Node operator[](const string&)  except+;
    Node operator[](long long)      except+;
  cdef Node LoadFile(const string&) except+;

cdef extern from "yaml_cpp_util.h":
  cdef const Node yamlNodeFind[K](Node &n, const K &k) except+;
  cdef void yamlNodeSet[K,V](Node &n, const K &k, V &v) except+;
  cdef bool boolNode(Node &) except+;
  cdef string c_emitNode "emitNode"(const Node &) except+;
