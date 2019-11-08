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
#include <stdio.h>
#include <getopt.h>
#include <iostream>
#include <yaml-cpp/yaml.h>

static void usage(const char *nm)
{
	fprintf(stderr,"usage: %s [-hc] [-C config_file] [-L config_file] -r root_node_name\n", nm);
	fprintf(stderr,"           -h                   : this message\n");
	fprintf(stderr,"           -c                   : config dump\n");
	fprintf(stderr,"           -C <config_file>     : config dump from file\n");
	fprintf(stderr,"           -L <config_file>     : config load from file\n");
	fprintf(stderr,"           -Y <yaml_file>       : load YAML definition from file\n");
	fprintf(stderr,"           -r <root_node_name>  : node in input YAML file to use for the root node\n");
	fprintf(stderr,"           -q                   : don't dump expanded YAML, just load.\n");
	fprintf(stderr,"           -t                   : track unrecognized YAML keys.\n");
	fprintf(stderr,"  Read YAML from stdin (or -Y file), build hierarchy and dump YAML on stdout\n");
}

int
main(int argc, char **argv)
{
int opt;
int             rval = 1;
int             cd   = 0;
const char *confdnam = 0;
const char *rootname = 0;
const char *conflnam = 0;
const char *defflnam = 0;
int            quiet = 0;
int            track = 0;
	while ( (opt = getopt(argc, argv, "hcC:r:L:Y:qt")) > 0 ) {
		switch ( opt ) {
			case 'c':
				cd = 1;
			break;

			case 'C':
				cd       = 1;
				confdnam = optarg;
			break;

			case 'L':
				conflnam = optarg;
			break;

			case 'h':
				rval = 0;
				// fall through
			default:
				usage(argv[0]);
				return rval;

			case 'q':
				quiet    = 1;
			break;

			case 'r':
				rootname = optarg;
			break;

			case 't':
				track    = 1;
			break;

			case 'Y':
				defflnam = optarg;
			break;
		}
	}
	if ( ! rootname ) {
		fprintf(stderr,"Need '-r root_node_name' option\n");
		return 1;
	}
	try {
		setCPSWVerbosity( "yaml", track );
		Dev r( defflnam ? IYamlSupport::buildHierarchy(defflnam, rootname) : IYamlSupport::buildHierarchy(std::cin, rootname) );
		Path p;

		if ( conflnam || cd ) {
			p = IYamlSupport::startHierarchy( r );
		}

		if ( conflnam ) {
			YAML::Node conf( YAML::LoadFile( conflnam ) );
			p->loadConfigFromYaml( conf );
		}

		if ( cd ) {
			YAML::Node n;
			if ( confdnam ) {
				n = YAML::LoadFile( confdnam );
			}
			p->dumpConfigToYaml( n );
			YAML::Emitter e;
			e << n;

			std::cout << e.c_str() << "\n";
		} else {
			if ( ! quiet ) {
				IYamlSupport::dumpYamlFile(
						r,
						0,
						rootname);
			} else {
				std::cout << "YAML Loaded without errors\n";
			}
		}
	} catch ( CPSWError &e ) {
		std::cerr << "CPSW Error: " << e.getInfo() << "\n";
		if ( quiet )
			return 1;
		throw;
	}
	if ( track ) {
		unsigned long n = IYamlSupport::getNumberOfUnrecognizedKeys();
		printf( "Unrecognized YAML Keys: %lu\n", n );
		if ( n ) {
			return 2;
		}
	}
	return 0;
}
