 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>

class TestFailed {};

static const char *yaml =
"#schemaversion 3.0.0\n"
"root:\n"
"  class: NullDev\n"
"  size:  1024\n"
"  children:\n"
"    seq1:\n"
"      class: SequenceCommand\n"
"      myCommentJunk:\n"
"      sequence:\n"
"        - { entry: aber, value: 1, mySeqJunk: 8 }\n"
"      at: { nelms: 1, myAddressJunk: 4 }\n"
"    seq2:\n"
"      class: SequenceCommand\n"
"      sequence:\n"
"        - [ { entry: aber, value: 1, mySeqSeqJunk: \"A\" } ]\n"
"        - [ { entry: aber, value: 1 } ]\n"
"      at: { nelms: 1 }\n"
"      enums:\n"
"        - { name: zero, value: 0 }\n"
"        - { name: one , value: 1, myEnumJunk: 4 }\n"
;

#define EXPECTED_KEYS 5

int
main(int argc, char **argv)
{
	try {
		unsigned long k;
		// Track unused YAML keys
		setCPSWVerbosity( "yaml", 1 );
		IPath::loadYamlStream( yaml );
		if ( (k = IYamlSupport::getNumberOfUnrecognizedKeys()) != EXPECTED_KEYS ) {
			fprintf(stderr, "Expected %d unrecognized keys but found %lu\n", EXPECTED_KEYS, k);
			printf("Test FAILED\n");
			throw TestFailed();
		}
	} catch (CPSWError &e) {
		fprintf(stderr, "CPSW Error caught: %s\n", e.what());
		printf("Test FAILED\n");
		throw TestFailed();
	}

	printf("Test PASSED\n");

	return 0;
}
