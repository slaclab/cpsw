 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_YAML_MERGE_H
#define CPSW_YAML_MERGE_H

#include <yaml-cpp/yaml.h>
#include <string.h>

namespace cpsw {

struct PNode;

struct PNode {
public:
	const PNode       *parent_;
	const YAML::Node   key_;

	PNode(const PNode *parent, const YAML::Node &key)
	: parent_ ( parent ),
	  key_    ( key    )
	{
	}
};

// resolve merge keys (recursively) under 'n'. I.e., perform
// merges and remove the merge key.
void resolveMergeKeys(YAML::Node n);

extern const char * const YAML_MERGE_KEY_PATTERN;

};

#endif
