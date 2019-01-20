//@C Copyright Notice
//@C ================
//@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
//@C file found in the top-level directory of this distribution and at
//@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
//@C
//@C No part of CPSW, including this file, may be copied, modified, propagated, or
//@C distributed except according to the terms contained in the LICENSE.txt file.

//
// Trivial wrapper for yaml-cpp. This provides just enough
// functionality to implement basic YamlFixup objects in
// python.
// This is *not* intended to rewrite much YAML but to tweak
// high-level details such as ip-address and the like...
//

// This silences the 'auto_ptr deprecated' warnings.
// See https://github.com/boostorg/python/issues/176
#define BOOST_NO_AUTO_PTR

#include <boost/python.hpp>
#include <yaml-cpp/yaml.h>

using namespace boost::python;
using YAML::Node;

template <typename Key>
static const YAML::Node
yamlNodeFind(const YAML::Node &n, const Key &k)
{
	// non-const operator[] inserts into YAML map!
	return static_cast<const YAML::Node>(n)[k];
}

static void setNodeNode(Node &lhs, const Node &rhs)
{
	lhs = rhs;
}

static void setNodeStr(Node &lhs, const std::string &rhs)
{
	lhs = rhs;
}

static void pushBackNode(Node &lhs, const Node &rhs)
{
	lhs.push_back( rhs );
}

static Node getitemStr(const Node &self, const std::string & key)
{
	return yamlNodeFind(self, key);
}

static Node getitemInt(const Node &self, int key)
{
	return yamlNodeFind(self, key);
}

static Node getitemNode(const Node &self, const Node &key)
{
	return yamlNodeFind(self, key);
}


static bool removeNode(Node &self, const Node &key)
{
	return self.remove(key);
}

static bool removeStr(Node &self, const std::string &key)
{
	return self.remove(key);
}

static void setitemNode(Node &self, const  Node &key, const Node &val)
{
	self[key] = val;
}

static void setitemInt(Node &self, int key, Node &val)
{
	self[key] = val;
}


static void setitemStr(Node &self, const Node &key, const std::string &val)
{
	self[key] = val;
}

static void setitemStrStr(Node &self, const std::string &key, const std::string &val)
{
	self[key] = val;
}


static const std::string asStr(const Node &self)
{
	return self.as<std::string>();
}

static bool boolNode(Node &self)
{
	return !! self;
}


static std::string emitNode(const YAML::Node &n)
{
YAML::Emitter e;

	e << n;

	return std::string( e.c_str() );
}

BOOST_PYTHON_MODULE(yaml_cpp)
{
	class_<Node> (
       "Node"
	)
	.def(init<const std::string &>())
	.def("IsDefined",        &Node::IsDefined)
	.def("IsNull",           &Node::IsNull)
	.def("IsScalar",         &Node::IsScalar)
	.def("IsSequence",       &Node::IsSequence)
	.def("IsMap",            &Node::IsMap)
    .def("Type",             &Node::Type)
	.def("Scalar",           &Node::Scalar, return_value_policy<copy_const_reference>() )
	.def("remove",           removeNode)
	.def("remove",           removeStr)
    .def("set",              setNodeNode)
    .def("set",              setNodeStr)
	.def("push_back",        pushBackNode)     
	.def("getAs",            asStr) // 'as' is a keyword in python
	.def("__iter__",         iterator<Node>())
	.def("__getitem__",      getitemStr)
	.def("__getitem__",      getitemInt)
	.def("__getitem__",      getitemNode)
	.def("__setitem__",      setitemNode)
	.def("__setitem__",      setitemInt)
	.def("__setitem__",      setitemStr)
	.def("__setitem__",      setitemStrStr)
	.def("__bool__",         boolNode)
	.def("LoadFile",         &YAML::LoadFile)
	.staticmethod("LoadFile")
	;

	class_<YAML::detail::iterator_value, bases<Node> >(
		"iterator_value"
	)
	.def_readwrite("first",  &YAML::detail::iterator_value::first)
	.def_readwrite("second", &YAML::detail::iterator_value::second)
	;

	enum_<YAML::NodeType::value> (
		"NodeType"
    )
	.value("Undefined", YAML::NodeType::Undefined)
	.value("Null",      YAML::NodeType::Null)
	.value("Scalar",    YAML::NodeType::Scalar)
	.value("Map",       YAML::NodeType::Map)
	.value("Sequence",  YAML::NodeType::Sequence)
	;

	def("emitNode", emitNode);
}
