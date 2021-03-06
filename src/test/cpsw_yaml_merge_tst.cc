 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <cpsw_yaml.h>
#include <cpsw_yaml_merge.h>
#include <iostream>
#include <stdio.h>

static const char *yaml=
"#schemaversion 3.0.0\n"
"default:\n"
"  root: &default\n"
"    " YAML_KEY_children ":\n"
"      main:\n"
"        " YAML_KEY_children ":\n"
"          merge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"          downmerge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"          upmerge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"          updownmerge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"          upupmerge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"          nomerge:\n"
"            " YAML_KEY_class ": something\n"
"            " YAML_KEY_instantiate ": false\n"
"\n"
"root:\n"
"  " YAML_KEY_MERGE ": *default\n"
"  " YAML_KEY_class ": Dev\n"
"  " YAML_KEY_size ": 1000\n"
"  " YAML_KEY_children ":\n"
"    " YAML_KEY_MERGE ":\n"
"      main:\n"
"        " YAML_KEY_MERGE ":\n"
"          " YAML_KEY_at ":\n"                    // main 'at'
"            " YAML_KEY_nelms ": 1\n"
"          " YAML_KEY_children ":\n"
"            " YAML_KEY_MERGE ":\n"
"              upupmerge:\n"
"                " YAML_KEY_MERGE ":\n"
"                  " YAML_KEY_at ":\n"
"                    " YAML_KEY_nelms ": 1\n"
"        " YAML_KEY_children ":\n"
"          upupmerge:\n"
"            " YAML_KEY_class ": Field\n"
"      secondchild:\n"
"        " YAML_KEY_class ": Field\n"
"        " YAML_KEY_MERGE ":\n"
"          " YAML_KEY_at ":\n"                    // secondchild 'at'
"            " YAML_KEY_MERGE ":\n"
"              " YAML_KEY_nelms ": 1\n"
"    main:\n"
"      " YAML_KEY_class ": Dev\n"
"      " YAML_KEY_size ":  100\n"
"      " YAML_KEY_MERGE ":\n"
"        " YAML_KEY_children ":\n"
"          " YAML_KEY_MERGE ":\n"
"            updownmerge:\n"
"              " YAML_KEY_class ": Field\n"
"              " YAML_KEY_at ":\n"                // updownmerge 'at'
"                " YAML_KEY_nelms ": 1\n"
"            merge:\n"
"              " YAML_KEY_at ":\n"                // merge 'at'
"                " YAML_KEY_nelms ": 44\n"
"              " YAML_KEY_size ":  88\n"
"              " YAML_KEY_class ": something\n"
"            upmerge:\n"
"              " YAML_KEY_at ":\n"
"                " YAML_KEY_nelms ": 1\n"
"          upmerge:\n"
"            " YAML_KEY_class ": Field\n"
"      " YAML_KEY_children ":\n"
"         " YAML_KEY_MERGE ": \n"
"            merge:\n"
"              " YAML_KEY_class ": Field\n"
"              " YAML_KEY_size ":\n"
"            " YAML_KEY_MERGE ":\n"
"              downmerge:\n"
"                " YAML_KEY_class ": Field\n"
"                " YAML_KEY_MERGE ":\n"
"                  " YAML_KEY_at ":\n"
"                    " YAML_KEY_nelms ": 1\n"   // downmerge 'at'
"         nomerge:\n"
"           " YAML_KEY_class ": Field\n"
"           " YAML_KEY_at ":\n"                 // nomerge 'at'
"             " YAML_KEY_nelms ": 1\n"
;

class dumph: public IPathVisitor {
public:
	virtual bool visitPre(ConstPath here)
	{
		std::cout << here->toString() << "\n";
		return true;
	}
	virtual void visitPost(ConstPath here)
	{
	}
};

static void pl(int l)
{
    for ( int i = 0; i < l; i++ )
      std::cout << ' ';
}

void rec (const YAML::Node &n, int l)
{
const YAML::Node &p ( n/*["peripherals"]*/ );

	if ( p ) {
		if ( p.IsScalar() ) {
            pl(l);
			std::cout << "%" << p << "\n";
		} else {
			for ( YAML::const_iterator it = p.begin(); it != p.end(); ++it ) {
                pl(l);
				if ( p.IsScalar() ) {
					std::cout << "scalar iteration\n";
				} else if ( p.IsMap() ) {
					std::cout << it->first << "-> " << "\n";
					rec( it->second, l+1 );
				} else {
					std::cout << "---- \n";
					rec( *it, l+1 );
				}
			}
		}
	}
}


class TestFailed {
public:
	int lineno;
	TestFailed( int lno )
	: lineno( lno )
	{}
};

static void createPathShouldFail(Hub h, const char *name, int lno)
{
	try {
		h->findByName( name );
		throw TestFailed( lno );
	} catch (NotFoundError &e) {
	}
}

const char *paths[] = {
	"main/merge",
	"main/nomerge",
	"main/downmerge",
	"main/upmerge",
	"main/updownmerge",
	"main/upupmerge",
	0
};

void dump(YAML::Node n)
{
YAML::Emitter out;
	out << n;
	std::cout << "###### BEGIN YAML DUMP ########\n";
	std::cout << out.c_str() << "\n";
	std::cout << "###### END YAML DUMP ########\n";
}

int
main(int argc, char **argv)
{
std::string str( yaml );
	printf("### ORIGINAL ###\n%s\n### ORIGINAL END ###\n", yaml);
	{
		YAML::Node root;
		{
		std::stringstream sstrm( str );
		root.reset( CYamlFieldFactoryBase::loadPreprocessedYaml( sstrm, NULL, false ) );
		}

		cpsw::resolveMergeKeys( root );

		dump( root );

		Hub top( CYamlFieldFactoryBase::dispatchMakeField( root, "root" ) );

		dumph visi;

		IPath::create( top )->explore( &visi );

#ifdef DEBUG
        rec( root, 0 );
#endif
int i;

		// 'default' node sets 'instantiate: false' everywhere

		for ( i=0; paths[i]; i++ ) {
			createPathShouldFail(top, paths[i], __LINE__);
		}

		{
		std::stringstream sstrm( str );
		root.reset( CYamlFieldFactoryBase::loadPreprocessedYaml( sstrm, NULL, false ) );
		}
		// remove merge of the default node
		root["root"].remove( YAML_KEY_MERGE );
		cpsw::resolveMergeKeys( root );
		// all fields should now be present
		top = CYamlFieldFactoryBase::dispatchMakeField( root, "root" );

		for ( i=0; paths[i]; i++ ) {
			top->findByName( paths[i] );
		}

		// check if the default 'nelms' is merged from upstream
		if ( top->findByName( "main/merge" )->tail()->getNelms() != 44 )
			throw TestFailed( __LINE__ );

		// check if the default/merged 'size' is overridden and erased
		if ( top->findByName( "main/merge" )->tail()->getSize() != IField::DFLT_SIZE )
			throw TestFailed( __LINE__ );

		{
		std::stringstream sstrm( str );
		root.reset( CYamlFieldFactoryBase::loadPreprocessedYaml( sstrm, NULL, false ) );
		}
		// remove merge of the default node
		root["root"].remove( YAML_KEY_MERGE );
		// if we set 'instantiate=false' then '/main/merge' must not
		// be created - even though a default is merged upstream
		root["root"][YAML_KEY_children]["main"][YAML_KEY_children][YAML_KEY_MERGE]["merge"][YAML_KEY_instantiate]=false;
		cpsw::resolveMergeKeys( root );

		top = CYamlFieldFactoryBase::dispatchMakeField( root, "root" );

		for ( i=0; paths[i]; i++ ) {
			try {
				top->findByName( paths[i] );
				if ( 0 == i ) {
					std::cout << "found paths[0]\n";
					throw TestFailed( __LINE__ );
				}
			} catch (NotFoundError &e) {
				if ( 0 != i ) {
					std::cout << "not found paths[" << i <<"]\n";
					throw TestFailed( __LINE__ );
				}
			}
		}

	std::cout << argv[0] << " - success\n";

	}


}
