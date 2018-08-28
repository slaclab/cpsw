 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <cpsw_yaml_keydefs.h>

static const char *yamlFmt=
"#schemaversion 3.0.0\n"
"  root:\n"
"    "YAML_KEY_class":   MemDev\n"
"    "YAML_KEY_size":      1024\n"
"    "YAML_KEY_children":\n"
"      mmio:\n"
"        "YAML_KEY_class":   MMIODev\n"
"        "YAML_KEY_size":      1024\n"
"        "YAML_KEY_byteOrder": LE\n"
"        "YAML_KEY_at":\n"
"          "YAML_KEY_nelms": 1\n"
"        "YAML_KEY_children":\n"
"          valLE:\n"
"            "YAML_KEY_class":    IntField\n"
"            "YAML_KEY_sizeBits": 128\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0\n"
"              "YAML_KEY_nelms":  1\n"
"          valBE:\n"
"            "YAML_KEY_class":    IntField\n"
"            "YAML_KEY_sizeBits": 128\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0\n"
"              "YAML_KEY_nelms":  1\n"
"              "YAML_KEY_byteOrder": BE\n"
"          raw:\n"
"            "YAML_KEY_class":    IntField\n"
"            "YAML_KEY_sizeBits": 8\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0\n"
"              "YAML_KEY_nelms":  16\n"
;

class TestFailed {};

int
main(int argc, char **argv)
{

	try {
		Path root = IPath::loadYamlStream(yamlFmt);

		ScalVal raw = IScalVal::create( root->findByName("mmio/raw") );

		uint8_t buf[ raw->getNelms() ];
		for (uint8_t j = 0; j < sizeof(buf)/sizeof(buf[0]); j++ ) {
			buf[j] = j;
		}
		raw->setVal(buf, sizeof(buf)/sizeof(buf[0]));

		ScalVal_RO valLE = IScalVal_RO::create( root->findByName("mmio/valLE") );
		ScalVal_RO valBE = IScalVal_RO::create( root->findByName("mmio/valBE") );

		uint64_t vu64;
		CString  cstr;

		vu64 = 0;
		valLE->getVal( &vu64, 1 );

		if ( vu64 != 0x0706050403020100 ) {
			fprintf(stderr,"Readback of LE as uint64_t FAILED (got 0x%08llx)\n", (unsigned long long)vu64);
			throw TestFailed();
		}

		vu64 = 0;
		valBE->getVal( &vu64, 1 );

		if ( vu64 != 0x08090a0b0c0d0e0f ) {
			fprintf(stderr,"Readback of BE as uint64_t FAILED (got 0x%08llx)\n", (unsigned long long)vu64);
			throw TestFailed();
		}

		valLE->getVal( &cstr, 1 );
		if ( strcmp( cstr->c_str(), "0x0f0e0d0c0b0a09080706050403020100" ) ) {
			fprintf(stderr,"Readback of LE as CString FAILED (got %s)\n", cstr->c_str());
			throw TestFailed();
		}

		valBE->getVal( &cstr, 1 );
		if ( strcmp( cstr->c_str(), "0x000102030405060708090a0b0c0d0e0f" ) ) {
			fprintf(stderr,"Readback of BE as CString FAILED (got %s)\n", cstr->c_str());
			throw TestFailed();
		}


	} catch (CPSWError &e) {
		fprintf(stderr,"Test Failed: %s\n", e.what());
		throw;
	}
	fprintf(stderr,"Test PASSED\n");
	return 0;
}
