#include <cpsw_api_user.h>
#include <yaml-cpp/yaml.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>

#include <cpsw_yaml.h>

#define TOP "cpsw_yaml_preproc_tst_1.yaml"
#define INC "cpsw_yaml_preproc_tst_2.yaml"

const char *topyaml =
"#\n"
"#once main\n"
"#include "INC"\n"
"#\n"
"\n"
"root:\n"
"  class: Dev\n"
"  name:  main\n"
"  children:\n"
"    - <<: *anchor\n"
"      name: overridename\n";

const char *incyaml =
"#\n"
"#once include\n"
"#include "TOP"\n"
"#\n"
"\n"
"leaf:   &anchor\n"
"  name:  origname\n"
"  nelms: 5\n"
"  class:\n"          // Ordered list of class hierarchy
"    - MyField1\n"    
"    - MyField2\n"
"    - Field\n";

const char *noincincyaml =
"#\n"
"#once include\n"
"#\n"
"\n"
"leaf:   &anchor\n"
"  name:  origname\n"
"  nelms: 5\n"
"  class:\n"          // Ordered list of class hierarchy
"    - MyField\n"    
"    - Field\n";


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
	printf("%s - success\n", argv[0]);

} catch (CPSWError &e) {
	std::cout << "CPSW Error: " << e.getInfo() << "\n";
	throw;
}
}
