#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <stdio.h>
#include <iostream>

static void usage(const char *nm)
{
	fprintf(stderr,"usage: %s [-hc] -r root_node_name\n", nm);
	fprintf(stderr,"           -h                   : this message\n");
	fprintf(stderr,"           -c                   : config dump\n");
	fprintf(stderr,"           -C <config_file>     : config dump from file\n");
	fprintf(stderr,"           -r <root_node_name>  : node in input YAML file to use for the root node\n");
	fprintf(stderr,"  Read YAML from stdin, build hierarchy and dump YAML on stdout\n");
}

int
main(int argc, char **argv)
{
int opt;
int             rval = 1;
int             cd   = 0;
const char *conffnam = 0;
const char *rootname = 0;
	while ( (opt = getopt(argc, argv, "hcC:r:")) > 0 ) {
		switch ( opt ) {
			case 'c':
				cd = 1;
			break;

			case 'C':
				cd = 1;
				conffnam=optarg;
			break;
			case 'h':
				rval = 0;
				// fall through
			default:
				usage(argv[0]);
				return rval;

			case 'r':
				rootname = optarg;
			break;
		}
	}
	if ( ! rootname ) {
		fprintf(stderr,"Need '-r root_node_name' option\n");
		return 1;
	}
	try {
		Hub h( IHub::loadYamlStream(std::cin, rootname) );
		if ( cd ) {
			YAML::Node n;
			if ( conffnam ) {
				n = YAML::LoadFile( conffnam );
			}
			Path p( IPath::create( h ) );
			p->dumpConfigToYaml( n );
			YAML::Emitter e;
			e << n;

			std::cout << e.c_str() << "\n";
		} else {
		IYamlSupport::dumpYamlFile(
				h,
				0,
				rootname);
		}
	} catch ( CPSWError &e ) {
		std::cerr << "CPSW Error: " << e.getInfo() << "\n";
		throw;
	}
	return 0;
}
