#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <cpsw_yaml.h>
#include <cpsw_yaml_keydefs.h>
#include <iostream>

static const char *yaml=
"default:\n"
"  root: &default\n"
"    "_YAML_KEY_children":\n"
"      main:\n"
"        "_YAML_KEY_children":\n"
"          merge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"          downmerge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"          upmerge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"          updownmerge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"          upupmerge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"          nomerge:\n"
"            "_YAML_KEY_class": something\n"
"            "_YAML_KEY_instantiate": false\n"
"\n"
"root:\n"
"  "_YAML_KEY_MERGE": *default\n"
"  "_YAML_KEY_class": Dev\n"
"  "_YAML_KEY_size": 1000\n"
"  "_YAML_KEY_children":\n"
"    "_YAML_KEY_MERGE":\n"
"      main:\n"
"        "_YAML_KEY_MERGE":\n"
"          "_YAML_KEY_at":\n"                    // main 'at'
"            "_YAML_KEY_nelms": 1\n"
"          "_YAML_KEY_children":\n"
"            "_YAML_KEY_MERGE":\n"
"              upupmerge:\n"
"                "_YAML_KEY_MERGE":\n"
"                  "_YAML_KEY_at":\n"
"                    "_YAML_KEY_nelms": 1\n"
"        "_YAML_KEY_children":\n"
"          upupmerge:\n"
"            "_YAML_KEY_class": Field\n"
"      secondchild:\n"
"        "_YAML_KEY_class": Field\n"
"        "_YAML_KEY_MERGE":\n"
"          "_YAML_KEY_at":\n"                    // secondchild 'at'
"            "_YAML_KEY_MERGE":\n"
"              "_YAML_KEY_nelms": 1\n"
"    main:\n"
"      "_YAML_KEY_class": Dev\n"
"      "_YAML_KEY_size":  100\n"
"      "_YAML_KEY_MERGE":\n"
"        "_YAML_KEY_children":\n"
"          "_YAML_KEY_MERGE":\n"
"            updownmerge:\n"
"              "_YAML_KEY_class": Field\n"
"              "_YAML_KEY_at":\n"                // updownmerge 'at'
"                "_YAML_KEY_nelms": 1\n"
"            merge:\n"
"              "_YAML_KEY_at":\n"                // merge 'at'
"                "_YAML_KEY_nelms": 44\n"
"              "_YAML_KEY_size":  88\n"
"              "_YAML_KEY_class": something\n"
"            upmerge:\n"
"              "_YAML_KEY_at":\n"
"                "_YAML_KEY_nelms": 1\n"
"          upmerge:\n"
"            "_YAML_KEY_class": Field\n"
"      "_YAML_KEY_children":\n"
"         "_YAML_KEY_MERGE": \n"
"            merge:\n"
"              "_YAML_KEY_class": Field\n"
"              "_YAML_KEY_size":\n"
"            "_YAML_KEY_MERGE":\n"
"              downmerge:\n"
"                "_YAML_KEY_class": Field\n"
"                "_YAML_KEY_MERGE":\n"
"                  "_YAML_KEY_at":\n"
"                    "_YAML_KEY_nelms": 1\n"   // downmerge 'at'
"         nomerge:\n"
"           "_YAML_KEY_class": Field\n"
"           "_YAML_KEY_at":\n"                 // nomerge 'at'
"             "_YAML_KEY_nelms": 1\n"
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


class TestFailed {};

static void createPathShouldFail(Hub h, const char *name)
{
	try {
		h->findByName( name );
		throw TestFailed();
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

int
main(int argc, char **argv)
{
std::string str( yaml );
std::stringstream sstrm( str );
	try {
		YAML::Node root( CYamlFieldFactoryBase::loadPreprocessedYaml( sstrm ) );


		Hub top( CYamlFieldFactoryBase::dispatchMakeField( root, "root" ) );

		dumph visi;

		IPath::create( top )->explore( &visi );

#ifdef DEBUG
        rec( root, 0 );
#endif
int i;

		// 'default' node sets 'instantiate: false' everywhere

		for ( i=0; paths[i]; i++ ) {
			createPathShouldFail(top, paths[i]);
		}

		// remove merge of the default node
		root["root"].remove( _YAML_KEY_MERGE );

		// all fields should now be present
		top = CYamlFieldFactoryBase::dispatchMakeField( root, "root" );

		for ( i=0; paths[i]; i++ ) {
			top->findByName( paths[i] );
		}

		// check if the default 'nelms' is merged from upstream
		if ( top->findByName( "main/merge" )->tail()->getNelms() != 44 )
			throw TestFailed();

		// check if the default/merged 'size' is overridden and erased
		if ( top->findByName( "main/merge" )->tail()->getSize() != IField::DFLT_SIZE )
			throw TestFailed();

		// if we set 'instantiate=false' then '/main/merge' must not
		// be created - even though a default is merged upstream
		root["root"][_YAML_KEY_children]["main"][_YAML_KEY_children][_YAML_KEY_MERGE]["merge"][_YAML_KEY_instantiate]=false;

		top = CYamlFieldFactoryBase::dispatchMakeField( root, "root" );

		for ( i=0; paths[i]; i++ ) {
			try {
				top->findByName( paths[i] );
				if ( 0 == i ) {
					std::cout << "found paths[0]\n";
					throw TestFailed();
				}
			} catch (NotFoundError &e) {
				if ( 0 != i ) {
					std::cout << "not found paths[" << i <<"]\n";
					throw TestFailed();
				}
			}
		}

	std::cout << argv[0] << " - success\n";

	} catch ( CPSWError &e ) {
		std::cout << "CPSW Error: " << e.getInfo() << "\n";
		throw;
	}


}
