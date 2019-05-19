#include <cpsw_yaml_merge.h>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <stdio.h>
#include <getopt.h>

static void usage(const char *nm)
{
	fprintf(stderr,"usage: %s [-Y] <top_yaml_file_name> [-h]\n", nm);
	fprintf(stderr,"    -h           :   this message\n");
	fprintf(stderr,"    -Y <filename>:   operate on <filename>\n");
	fprintf(stderr,"       <filename>:   same as -Y <filename>\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"          resolve all merge keys ('%s') found in a hierarchy of YAML 'Maps'\n", cpsw::YAML_MERGE_KEY_PATTERN);
	fprintf(stderr,"          <top_yaml_file_name> points to a file which must define a Map!\n");
	fprintf(stderr,"          Merge keys are resolved recursively in all maps that are sub-maps\n");
	fprintf(stderr,"          of the top-level maps (which arbitrarily deep levels) but the\n");
	fprintf(stderr,"          program does not cross the boundary of the map-in-map hierarchy:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"              top:                  \n");
	fprintf(stderr,"                <<: &map1           \n");
	fprintf(stderr,"                  k1:               \n");
	fprintf(stderr,"                    <<: &map2       \n");
	fprintf(stderr,"                      k2: v2        \n");
	fprintf(stderr,"                      k3:           \n");
	fprintf(stderr,"                       -            \n");
	fprintf(stderr,"                        <<: &map3   \n");
	fprintf(stderr,"                          k4: v4    \n");
	fprintf(stderr,"\n");
	fprintf(stderr,"          the maps 'map1' and 'map2' would be merged but not 'map3'\n");
	fprintf(stderr,"          because it is not a sub-map of 'top'. The result would be:\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"              top:                  \n");
	fprintf(stderr,"                k1:                 \n");
	fprintf(stderr,"                  k2: v2            \n");
	fprintf(stderr,"                  k3:               \n");
	fprintf(stderr,"                   - <<:            \n");
	fprintf(stderr,"                      k4: v4        \n");
}

int
main(int argc, char **argv)
{
int         opt;
const char *fn = 0;

	while ( (opt = getopt(argc, argv, "hY:")) >= 0 ) {
		switch ( opt ) {
			default:
			case 'h':
				usage(argv[0]);
				return 0;
			case 'Y':
				fn = optarg;
				break;
		}
	}

	if ( optind >= argc && ! fn ) {
		fprintf(stderr,"YAML file name missing\n");
		usage( argv[0] );
		return 1;
	} else {
		fn = argv[optind];
	}

	YAML::Node root = YAML::LoadFile( fn );

	if ( ! root.IsDefined() || ! root.IsMap() ) {
		fprintf(stderr, "Bad root node -- not a map\n");
		return 1;
	}

	cpsw::resolveMergeKeys( root );

	YAML::Emitter out;
	out << root;
	std::cout << out.c_str() << "\n";

	return 0;
}
