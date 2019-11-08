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
#include <yaml-cpp/yaml.h>
#include <cpsw_api_user.h>

static void usage(const char *nm)
{
const char *justTheName = ::strrchr(nm, '/');
	if ( !justTheName )
		justTheName = nm;
	else
		justTheName++;
	fprintf(stderr,"usage: %s [-D <yaml_dir>] [-Y <yaml_file>] [-vhV]\n", justTheName);
	fprintf(stderr,"  -Y <yaml_file>: Preprocess YAML file <yaml_file> (or stdin if no -Y given).\n");
	fprintf(stderr,"  -D <yaml_dir> : Included files are searched for in <yaml_dir> or in the\n");
	fprintf(stderr,"                  'dirname' of <yaml_file> if no -D given.\n");
	fprintf(stderr,"                  In addition, directories listed in the environment variable\n");
	fprintf(stderr,"                  YAML_PATH are searched.\n");
	fprintf(stderr,"  -v            : Enable verbose mode; comments are added indicating the start\n");
	fprintf(stderr,"                  and end of included files as well as the schemaversion.\n");
	fprintf(stderr,"  -p            : print preprocessed YAML to stdout and parse it as well.\n");
	fprintf(stderr,"                  Useful for catching syntax errors...\n");
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
int             rval = 1;
bool            verb = false;
bool            pars = false;

	while ( (opt = getopt(argc, argv, "D:Y:hvpV")) > 0 ) {
		switch (opt) {
			case 'h':
				rval = 0;
			default:
				usage(argv[0]);
				return rval;

			case 'v': verb = true;       break;
			case 'p': pars = true;       break;

			case 'D': yaml_dir = optarg; break;
			case 'Y': yaml_fil = optarg; break;

			case 'V':
				fprintf(stdout, "%s using CPSW version: %s\n", argv[0], getCPSWVersionString());
				return 0;
		}
	}


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
		snprintf( line, sizeof(line), "#schemaversion %d.%d.%d\n",
		           preprocessor.getSchemaVersionMajor(),
		           preprocessor.getSchemaVersionMinor(),
		           preprocessor.getSchemaVersionRevision()
		        );
		addHeader.pushbuf( StreamMuxBuf::mkstrm( std::string( line ) ), NULL, 1 );
	}
	addHeader.pushbuf( preprocessedStream, NULL, 0 );

	std::istream streamWithHeader( &addHeader );

	if ( pars ) {
		std::stringstream tmp;
		tmp << streamWithHeader.rdbuf();
		std::cout << tmp.str();
		YAML::Load( tmp );
	} else {
		std::cout << streamWithHeader.rdbuf();
	}

	return 0;
}

