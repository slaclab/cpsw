#include <cpsw_api_user.h>
#include <cpsw_api_builder.h>
#include <stdio.h>
#include <iostream>

static void usage(const char *nm)
{
	fprintf(stderr,"usage: %s [-h] -r root_node_name\n", nm);
	fprintf(stderr,"           -h                   : this message\n");
	fprintf(stderr,"           -r <root_node_name>  : node in input YAML file to use for the root node\n");
	fprintf(stderr,"  Read YAML from stdin, build hierarchy and dump YAML on stdout\n");
}

int
main(int argc, char **argv)
{
int opt;
int             rval = 1;
const char *rootname = 0;
	while ( (opt = getopt(argc, argv, "hr:")) > 0 ) {
		switch ( opt ) {
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
 IYamlSupport::dumpYamlFile(
	IHub::loadYamlStream(std::cin, rootname),
	0,
	rootname);

	} catch ( CPSWError &e ) {
		std::cerr << "CPSW Error: " << e.getInfo() << "\n";
		throw;
	}
	return 0;
}
