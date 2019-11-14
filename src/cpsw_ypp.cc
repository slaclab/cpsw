 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <cpsw_preproc.h>
#include <iostream>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <cpsw_api_builder.h>
#include <fstream>
#include <sstream>
#include <typeinfo>
#include <cxxabi.h>

static void usage(const char *nm)
{
const char *justTheName = ::strrchr(nm, '/');
	if ( !justTheName )
		justTheName = nm;
	else
		justTheName++;
	fprintf(stderr,"usage: %s [-D <yaml_dir>] [-P <preprocessed_file_name>] [-hpvV] [-Y] <yaml_file>\n", justTheName);
	fprintf(stderr,"  -Y <yaml_file>: Preprocess YAML file <yaml_file> (or stdin the file name is '-')\n");
	fprintf(stderr,"  -D <yaml_dir> : Included files are searched for in <yaml_dir> or in the\n");
	fprintf(stderr,"                  'dirname' of <yaml_file> if no -D given.\n");
	fprintf(stderr,"                  In addition, directories listed in the environment variable\n");
	fprintf(stderr,"                  YAML_PATH are searched.\n");
	fprintf(stderr,"  -v            : Enable verbose mode; comments are added indicating the start\n");
	fprintf(stderr,"                  and end of included files.\n");
	fprintf(stderr,"  -P <prep_file>: print preprocessed YAML to '<prep_file>' (or stdout if the file name is '-')\n");
	fprintf(stderr,"  -p            : parse preprocessed YAML\n");
	fprintf(stderr,"                  Useful for catching syntax errors...\n");
	fprintf(stderr,"  -l <cpsw_root>: print preprocessed YAML to stdout (or -P <file>) and load CPSW hierarchy\n");
	fprintf(stderr,"                  (using <cpsw_root>) as well. Useful for debugging:\n");
	fprintf(stderr,"                    - parser detects syntax errors\n");
	fprintf(stderr,"                    - CPSW detects missing (mandatory)\n");
	fprintf(stderr,"                    - CPSW reports unrecognized keys\n");
	fprintf(stderr,"                  NOTE: the name of the CPSW hierarchy's root must be passed to this option\n");
	fprintf(stderr,"  -h            : Print this message.\n");
	fprintf(stderr,"  -V            : Print version info and exit\n");
}

class NoOpDeletor {
public:
	void operator()(StreamMuxBuf::Stream::element_type *obj)
	{
	}
};

int
main(int argc, char **argv)
{
int              opt;
const char      *sep;
std::string main_dir;
const char *yaml_dir = 0;
const char *yaml_fil = 0;
const char *prep_fil = 0;
int             rval = 1;
bool            verb = false;
bool            pars = false;
const char     *load = 0;
std::ofstream  prepf;
std::ostream  *preps = 0;

	while ( (opt = getopt(argc, argv, "D:Y:hl:vpP:V")) > 0 ) {
		switch (opt) {
			case 'h':
				rval = 0;
			default:
				usage(argv[0]);
				return rval;

			case 'v': verb = true;       break;
			case 'l': load = optarg;     break;
			case 'p': pars = true;       break;

			case 'D': yaml_dir = optarg; break;
			case 'P': prep_fil = optarg; break;
			case 'Y': yaml_fil = optarg; break;

			case 'V':
				fprintf(stdout, "%s using CPSW version: %s\n", argv[0], getCPSWVersionString());
				return 0;
		}
	}

	if ( optind < argc ) {
		yaml_fil = argv[optind];
	}

	if ( ! yaml_fil ) {
		fprintf(stderr, "Error: missing YAML file-name argument\n");
		usage( argv[0] );
		return rval;
	}

	if ( 0 == ::strcmp(yaml_fil, "-") ) {
		yaml_fil = 0;
	}

	try {


		if ( ! yaml_dir && yaml_fil && (sep = ::strrchr(yaml_fil, '/')) ) {
			main_dir = std::string(yaml_fil);
			main_dir.resize(sep - yaml_fil);
			yaml_dir = main_dir.c_str();
		}

		StreamMuxBuf::Stream top_strm;

		if ( yaml_fil ) {
			top_strm = StreamMuxBuf::mkstrm( yaml_fil );
		} else {
			top_strm = StreamMuxBuf::Stream( &std::cin, NoOpDeletor() );
		}

		StreamMuxBuf     muxer;
		// Use an additional MuxBuf to ensure the final YAML submitted to
		// the parser is read from a single stream so that line numbers
		// printed in error messages are accurate...
		StreamMuxBuf     addHeader;

		YamlPreprocessor preprocessor( top_strm, &muxer, yaml_dir );

		preprocessor.setVerbose( verb );

		preprocessor.process();

		StreamMuxBuf::Stream preprocessedStream( new std::istream( &muxer ) );

		if ( preprocessor.getSchemaVersionMajor() >= 0 ) {
			char line[256];
			snprintf( line, sizeof(line), "#schemaversion %d.%d.%d\n\n",
					preprocessor.getSchemaVersionMajor(),
					preprocessor.getSchemaVersionMinor(),
					preprocessor.getSchemaVersionRevision()
					);
			addHeader.pushbuf( StreamMuxBuf::mkstrm( std::string( line ) ), NULL, 1 );
		}
		addHeader.pushbuf( preprocessedStream, NULL, 0 );

		std::istream streamWithHeader( &addHeader );

		if ( prep_fil ) {
			if ( 0 == ::strcmp( prep_fil, "-" ) ) {
				preps = &std::cout;
			} else {
				prepf.open( prep_fil, std::ios_base::out | std::ios_base::trunc );
				if ( ! prepf.is_open() || prepf.bad() ) {
					std::string msg = std::string( "Unable to open '" ) + prep_fil + "'";
					perror( msg.c_str() );
					goto bail;
				}
				preps = &prepf;
			}
		}

		if ( pars || load ) {
			std::stringstream tmp;
			tmp << streamWithHeader.rdbuf();
			if ( preps ) {
				(*preps) << tmp.str();
				if ( prep_fil ) {
					prepf.close();
				}
			}
			YamlNode top( IYamlSupport::loadYaml( tmp ) );
			if ( load ) {
				setCPSWVerbosity( "yaml", 1 );
				IYamlSupport::setSchemaVersion( top,
				                                preprocessor.getSchemaVersionMajor(),
				                                preprocessor.getSchemaVersionMinor(),
				                                preprocessor.getSchemaVersionRevision() );
				IYamlSupport::buildHierarchy( top, load );
			}
		} else {
			if ( preps ) {
				(*preps) << streamWithHeader.rdbuf();
			}
		}
	} catch ( std::exception &e ) {
		fprintf( stderr, "\nERROR -- Exception of type '%s' caught:\n\n  %s\n\n", abi::__cxa_demangle( typeid( e ).name(), 0, 0, 0 ), e.what() );
		goto bail;
	}

	rval = 0;

bail:
	if ( prep_fil && prepf.is_open() ) {
		prepf.close();
	}

	return rval;
}
