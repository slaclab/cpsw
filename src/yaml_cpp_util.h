//@CCopyright Notice
//@C================
//@CThis file is part of CPSW. It is subject to the license terms in the LICENSE.txt
//@Cfile found in the top-level directory of this distribution and at
//@Chttps://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
//@C
//@CNo part of CPSW, including this file, may be copied, modified, propagated, or
//@Cdistributed except according to the terms contained in the LICENSE.txt file.

#ifndef YAML_CPP_UTIL_H
#define YAML_CPP_UTIL_H

#include <yaml-cpp/yaml.h>

using YAML::Node;

template <typename Key>
static const YAML::Node
yamlNodeFind(const YAML::Node &n, const Key &k)
{
// non-const operator[] inserts into YAML map!
const YAML::Node &found( static_cast<const YAML::Node>(n)[k] );
// If nothing is found an undefined node is returned which cannot
// be assigned to anything...
	return found.IsDefined() ? found : Node();
}

template <typename Key, typename Val>
static void
yamlNodeSet(YAML::Node &n, const Key &k, Val &v)
{
	n[k] = v;
}

static bool
boolNode(Node &self)
{
	return !! self;
}

static std::string
emitNode(const YAML::Node &n)
{
YAML::Emitter e;

	e << n;

	return std::string( e.c_str() );
}

/*we want to avoid cython to use the yaml-cpp assignment operator
* as that may produce very weird results. I don't fully understand
* the copy semantics of 'node' objects.
*
* In particular: we want cython to avoid generating intermediate
* Nodes which produce weird results:
*
* Node a, b, tmp;
*
*   tmp = some_node;
*   a   = tmp;
*   tmp = some_other_node;
*   b   = tmp;
*
* results in a, b, and tmp all referring to 'some_other_node' whereas
*
*   a   = some_node;
*   b   = some_other_node;
*
* yields a 'normal' result. Seems that assigment to already defined nodes is
* problematic...
*
* Avoid temporary nodes introduce by cython by using pointers...
*
*/
static inline int
handleIterator(YAML::Node *n1p, YAML::Node *n2p, YAML::const_iterator *itp)
{
	if ( (**itp).IsDefined() ) {
		n1p->reset( **itp );
		return 1;
	} else {
		n1p->reset( (**itp).first  );
		n2p->reset( (**itp).second );
		return 2;
	}
}

static inline void
yamlNodeReset(YAML::Node *n1p, const YAML::Node *n2p)
{
	n1p->reset( *n2p );
}

static inline void
yamlNodeReset(YAML::Node *n1p, const YAML::Node &n2)
{
	n1p->reset( n2 );
}

#endif
