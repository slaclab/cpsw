 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <yaml-cpp/yaml.h>
#include <cpsw_yaml_merge.h>
#include <cpsw_error.h>
#include <string>

#include <stdio.h>
#include <string.h>

using cpsw::PNode;

#undef  YAML_MERGE_DEBUG

namespace cpsw {

const char * const YAML_MERGE_KEY_PATTERN = "<<";

static std::string pps(const PNode *p)
{
	if ( ! p )
		return std::string();
	if ( p->parent_ ) {
		return pps( p->parent_ ) + std::string( "/" ) + p->key_.Scalar();
	}
	return std::string("/") + p->key_.Scalar();
}

// merge node 'm' into 'n'

// for all children of 'm' (the node that is being merged)
//   (the children are 'key' : 'node' pairs)
//   - if a key is not found in 'n':
//       -> enter the child (key:node) as a new map entry in 'n'
//   - else (if the key *is* found in 'n' and) if m[key] is a map:
//       -> recurse into n[key], m[key]
//          i.e., keep the entry in 'n' (overriding the corresponding
//          merged node) but continue to look for children of m[key]
//          that might not exist in n[key] and thus need to be merged.

static void
mergeInto(const PNode *p, YAML::Node n, YAML::Node m)
{
	if ( ! m.IsMap() ) {
		throw InternalError( "Internal Error -- mergee no a map: " + pps( p ) );
	}
	if ( ! n.IsMap() ) {
		throw InternalError( "Internal Error -- merge target no a map: " + pps( p ) );
	}

	YAML::iterator it  = m.begin();
	YAML::iterator ite = m.end();

#ifdef YAML_MERGE_DEBUG
	int merges = 0;
#endif

	while ( it != ite ) {

		// make sure no key is inserted by using const ref.
		const YAML::Node &lkup( n );

#ifdef YAML_MERGE_DEBUG
		fprintf(stderr,"Looking for %s in %s\n", it->first.Scalar().c_str(), pps( p ).c_str());
#endif

		YAML::Node found( lkup[ it->first.Scalar() ] );

		if ( ! found.IsDefined() ) {
#ifdef YAML_MERGE_DEBUG
			merges++;
			fprintf(stderr,"merging %s into %s (defined %d)\n", it->first.Scalar().c_str(), pps( p ).c_str(), !!found.IsDefined());
#endif
			n[ it->first ] = it->second;
		} else {
			if ( it->second.IsMap() ) {
				PNode here( p, it->first );
				mergeInto( &here, found, it->second );
			}
		}

		++it;
	}

#ifdef YAML_MERGE_DEBUG
	if ( merges > 0 ) {
		fprintf(stderr,"After merging:\n");
		fprintf(stderr,"%s\n", pps(p).c_str());
		for (it = n.begin(); it != n.end(); ++it) {
			fprintf(stderr,"  '%s'\n", it->first.Scalar().c_str());
		}
	}
#endif
}

static void
resolveMergeKeys(const PNode *p, YAML::Node n)
{
YAML::iterator it  = n.begin();
YAML::iterator ite = n.end();
YAML::Node     merged_node;
YAML::Node     resolved_val;

    // first make sure that all merge keys are
    // resolved in all *children* (map entries) of
    // node 'n'
	while ( it != ite ) {

		PNode              here( p, it->first );
		const std::string &k( it->first.as<std::string>() );
		YAML::Node         v( it->second );

		if ( v.IsDefined() && v.IsMap() ) {

			resolveMergeKeys( &here, v );

			if ( 0 == ::strcmp( YAML_MERGE_KEY_PATTERN, k.c_str() ) ) {
				if ( ! merged_node.IsNull() ) {
					throw CPSWError(   std::string("yaml_merge: multiple ")
					                 + YAML_MERGE_KEY_PATTERN
					                 + " found here: "
					                 + pps( &here ) );
				}
				merged_node.reset( v );
			}

		} else {
			if ( 0 == ::strcmp( YAML_MERGE_KEY_PATTERN, k.c_str() ) ) {
				throw CPSWError( std::string("yaml_merge: merged node is not a map: " + pps( &here )) );
			}
#ifdef YAML_MERGE_DEBUG
			fprintf(stderr, "Leaf found: %s\n", pps( &here ).c_str());
#endif
		}
		++it;
	}

	// No merge keys are present further down the tree.
	// if a merge key is present in *this* map then
    // we must merge the tree attached to the merge
	// key into the rest of map entries of 'n'

	if ( ! merged_node.IsNull() ) {
		mergeInto( p, n, merged_node );
		n.remove( YAML_MERGE_KEY_PATTERN );
	}
}

void
resolveMergeKeys(YAML::Node n)
{
	resolveMergeKeys(0, n);
}

};
