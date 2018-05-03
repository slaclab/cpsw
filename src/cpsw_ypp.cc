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

static void usage(const char *nm)
{
const char *justTheName = ::strrchr(nm, '/');
	if ( !justTheName )
		justTheName = nm;
	else
		justTheName++;
	fprintf(stderr,"usage: %s [-D <yaml_dir>] [-Y <yaml_file>] [-vh]\n", justTheName);
	fprintf(stderr,"  -Y <yaml_file>: Preprocess YAML file <yaml_file> (or stdin if no -Y given).\n");
	fprintf(stderr,"  -D <yaml_dir> : Included files are searched for in <yaml_dir> or in the\n");
	fprintf(stderr,"                  'dirname' of <yaml_file> if no -D given.\n");
	fprintf(stderr,"  -v            : Enable verbose mode; comments are added indicating the start\n");
	fprintf(stderr,"                  and end of included files as well as the schemaversion.\n");
	fprintf(stderr,"  -h            : Print this message.\n");
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

	while ( (opt = getopt(argc, argv, "D:Y:hv")) > 0 ) {
		switch (opt) {	
			case 'h':
				rval = 0;
			default:
				usage(argv[0]);
				return rval;

			case 'v': verb = true;       break;

			case 'D': yaml_dir = optarg; break;
			case 'Y': yaml_fil = optarg; break;
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
YamlPreprocessor preprocessor( top_strm, &muxer, yaml_dir );

	if ( verb ) {
    	preprocessor.setVerbose( true );
	}

	preprocessor.process();

	std::istream preprocessed_stream( &muxer );

	if ( verb && preprocessor.getSchemaVersionMajor() >= 0 ) {
		std::cout << "#schemaversion "
		          << preprocessor.getSchemaVersionMajor()
		          << "."
		          << preprocessor.getSchemaVersionMinor()
		          << "."
		          << preprocessor.getSchemaVersionRevision()
		          << "\n";
	}

	std::cout << preprocessed_stream.rdbuf();

	return 0;
}

