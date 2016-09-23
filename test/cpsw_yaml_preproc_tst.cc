 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <yaml-cpp/yaml.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>

#include <cpsw_yaml.h>

#define TOP "cpsw_yaml_preproc_tst_1.yaml"
#define INC "cpsw_yaml_preproc_tst_2.yaml"
#define INC_V00 "cpsw_yaml_preproc_tst_inc_00.yaml"
#define INC_V30 "cpsw_yaml_preproc_tst_inc_30.yaml"
#define INC_V20 "cpsw_yaml_preproc_tst_inc_20.yaml"
#define INC_V31 "cpsw_yaml_preproc_tst_inc_31.yaml"

const char *v00 = "#\n";
const char *v30 = "#schemaversion 3.0.0\n";
const char *v20 = "#schemaversion 2.0.0\n";
const char *v31 = "#schemaversion 3.1.0\n";

const char *top00=
"#\n"
"#include "INC_V00"\n"
"root:\n"
"  "YAML_KEY_class": Dev\n";

const char *top03=
"#\n"
"#include "INC_V00"\n"
"#schemaversion 3.0.0\n"
"root:\n"
"  "YAML_KEY_class": Dev\n";

const char *top23=
"#\n"
"#include <"INC_V20">\n"
"#schemaversion 3.0.0\n"
"root:\n"
"  "YAML_KEY_class": Dev\n";

const char *top33=
"#\n"
"#include "INC_V31"\n"
"#schemaversion 3.0.0\n"
"root:\n"
"  "YAML_KEY_class": Dev\n";




const char *topyaml =
"#\n"
"#schemaversion 3.0.0\n"
"#once main\n"
"#include "INC"\n"
"#\n"
"\n"
"root:\n"
"  "YAML_KEY_class": Dev\n"
"  "YAML_KEY_children":\n"
"    overridename:\n"
"      "YAML_KEY_MERGE": *anchor\n"
;

const char *incyaml =
"#\n"
"#schemaversion 3.0.0\n"
"#once include\n"
"#include "TOP"\n"
"#\n"
"\n"
"leaf: &anchor\n"
"  "YAML_KEY_at":\n"
"    "YAML_KEY_nelms": 5\n"
"  "YAML_KEY_class":\n"          // Ordered list of class hierarchy
"    - MyField1\n"    
"    - MyField2\n"
"    - Field\n"
;

const char *noincincyaml =
"#\n"
"#schemaversion 3.0.0\n"
"#once include\n"
"#\n"
"\n"
"leaf:   &anchor\n"
"  "YAML_KEY_at":\n"
"    "YAML_KEY_nelms": 5\n"
"  "YAML_KEY_class":\n"          // Ordered list of class hierarchy
"    - MyField\n"    
"    - Field\n"
;


class TestFailed{};

static void wrf(const char *contents, const char *fnam)
{
FILE *f;
	if ( ! (f = fopen(fnam, "w")) ) {
		perror("unable to open file for writing\n");
		throw TestFailed();
	}
size_t l = strlen(contents);
	if ( l != fwrite(contents, 1, l, f) ) {
		perror("Unable to write all characters to include file");
		throw TestFailed();
	}
	if ( fclose(f) ) {
		perror("closing file");
		throw TestFailed();
	}
}

int
main(int argc, char **argv)
{
struct stat sb;
try {
    fprintf(stderr,"Note: Warning messages about MyFieldXX.so not being found are normal and expected\n");
	wrf(noincincyaml, INC);
	unlink(TOP);
	if ( 0 == stat(TOP, &sb) ) {
		fprintf(stderr,"Unable to remove top yaml file\n");
		throw TestFailed();
	}

	{
	// include file not including top should succeed
	Hub h( IHub::loadYamlStream( topyaml, "root" ) );
	}

	wrf(incyaml, INC);
	try {
		// include file includes 'top' file (which doesn't exist
		// since we read the top from memory); we expect
		// failure...
		Hub h( IHub::loadYamlStream( topyaml, "root" ) );
		fprintf(stderr,"Reading top should fail\n");		
		throw TestFailed();
	} catch (FailedStreamError &e) {
	}

	wrf(topyaml, TOP);

	{
	Hub h( IHub::loadYamlFile( TOP, "root" ) );
	// check merge key
	Path p( h->findByName("overridename") );
	if ( p->getNelms() != 5 )
		throw TestFailed();
	}

	wrf( v00, INC_V00 );
	wrf( v30, INC_V30 );
	wrf( v31, INC_V31 );
	wrf( v20, INC_V20 );

	// test combinations of schemaversions
	try {
		// No schemaversion should fail
		Hub h( IHub::loadYamlStream(top00, "root") );
		throw TestFailed();
	} catch (BadSchemaVersionError) {
	}

	try {
		// No no version and major should fail
		Hub h( IHub::loadYamlStream(top03, "root") );
		throw TestFailed();
	} catch (BadSchemaVersionError) {
	}

	try {
		// No mismatching major should fail
		Hub h( IHub::loadYamlStream(top23, "root") );
		throw TestFailed();
	} catch (BadSchemaVersionError) {
	}

	{
		// No matching majors should pass
		Hub h( IHub::loadYamlStream(top33, "root") );
	}


	printf("%s - success\n", argv[0]);

} catch (CPSWError &e) {
	std::cout << "CPSW Error: " << e.getInfo() << "\n";
	throw;
}
}
