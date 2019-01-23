//@C Copyright Notice
//@C ================
//@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
//@C file found in the top-level directory of this distribution and at
//@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
//@C
//@C No part of CPSW, including this file, may be copied, modified, propagated, or
//@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef YAML_CPP_UTIL_H
#define YAML_CPP_UTIL_H

#include <yaml-cpp/yaml.h>

using YAML::Node;

template <typename Key>
static const YAML::Node
yamlNodeFind(const YAML::Node &n, const Key &k)
{
	// non-const operator[] inserts into YAML map!
	return static_cast<const YAML::Node>(n)[k];
}

template<typename Key, typename Val>
static void
yamlNodeSet(YAML::Node &n, const Key &k, Val &v)
{
  n[k] = v; 
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

#endif
