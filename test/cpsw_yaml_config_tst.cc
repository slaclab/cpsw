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
#include <cpsw_mem_dev.h>
#include <cpsw_obj_cnt.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <getopt.h>
#include <yaml-cpp/yaml.h>

class TestFailed {
public:
	std::string msg_;

	TestFailed(const char *m)
	: msg_(m)
	{
	}
};

static const char *yamlFmt=
"#schemaversion 3.0.0\n"
"  root:\n"
"    "YAML_KEY_class":   MemDev\n"
"    "YAML_KEY_size":      1024\n"
"    "YAML_KEY_children":\n"
"      mmio:\n"
"        "YAML_KEY_class":   MMIODev\n"
"        "YAML_KEY_size":      1024\n"
"        "YAML_KEY_at":\n"
"          "YAML_KEY_nelms": 1\n"
"        "YAML_KEY_children":\n"
"          val1:\n"
"            "YAML_KEY_class":    IntField\n"
"            "YAML_KEY_sizeBits": 32\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0\n"
"              "YAML_KEY_nelms":  8\n"
"          val2:\n"
"            "YAML_KEY_class":       IntField\n"
"            "YAML_KEY_sizeBits":    32\n"
"            "YAML_KEY_instantiate": %s\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0x100\n"
"              "YAML_KEY_nelms":  8\n"
"          val3:\n"
"            "YAML_KEY_class":       IntField\n"
"            "YAML_KEY_sizeBits":    32\n"
"            "YAML_KEY_encoding":    IEEE_754\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0x200\n"
"              "YAML_KEY_nelms":  8\n"
"          val3i:\n"
"            "YAML_KEY_class":       IntField\n"
"            "YAML_KEY_sizeBits":    32\n"
"            "YAML_KEY_instantiate": %s\n"
"            "YAML_KEY_at":\n"
"              "YAML_KEY_offset": 0x200\n"
"              "YAML_KEY_nelms":  8\n"
;

int main(int argc, char **argv)
{
int rval = 1;
uint8_t *bufp;

try {
	Hub             h;
	ConstMemDevImpl mem;
	YAML::Node      cnfg;
	unsigned        val2Size = 0xdeadbeef;

	const char *passes[] = {
		"true",
		"false"
	};

	for ( unsigned pass = 0; pass < sizeof(passes)/sizeof(passes[0]); pass ++ ) {

		char yaml[10000];
		snprintf(yaml, sizeof(yaml), yamlFmt, passes[pass], passes[pass]);

		// during the second pass 'val2' and 'val3i' are not instantiated
		Path top = IPath::loadYamlStream( yaml );
		Hub    h = top->origin();

		ConstMemDevImpl mem = boost::dynamic_pointer_cast<ConstMemDevImpl::element_type>( h );

		bufp = mem->getBufp();

		memset(bufp, 0xaa, mem->getSize());

		ScalVal val1   = IScalVal::create( h->findByName("mmio/val1") );

		unsigned nelms = val1->getNelms();

		uint32_t v32[nelms];


		if ( 0 == pass ) {
			for ( unsigned i=0; i<nelms; i++ )
				v32[i] = (i<<24) | (i<<16) | (i<<8) | i;
	
			val1->setVal( v32, nelms );

			ScalVal   val2   = IScalVal::create( h->findByName("mmio/val2")  );
			ScalVal   val3i  = IScalVal::create( h->findByName("mmio/val3i") );

			val2Size       = val2->getNelms() * ((val2->getSizeBits() + 7 )/8);

			union {
				float    f;
				uint32_t u;
			} uu;

			uu.f = 8.5;

			val3i->setVal( uu.u );

			val2->setVal( (uint64_t) 0x00000000 );
		} else {
			top->loadConfigFromYaml( cnfg );	
		}

		// verify values
		for ( unsigned i=0; i<nelms; i++ ) {
			uint32_t vv;
			memcpy(&vv, bufp + i*sizeof(uint32_t), sizeof(uint32_t));
			if ( vv != ((i<<24) | (i<<16) | (i<<8) | i) )
				throw TestFailed("v1 readback");
		}

		for ( unsigned i=0; i<val2Size; i++ ) {
			if ( bufp[0x100+i] != (0 == pass ? 0x00 : 0xaa) ) {
				throw TestFailed("v2 readback");
			}
		}

		DoubleVal val3   = IDoubleVal::create( h->findByName("mmio/val3")  );
		double    dv3[ val3->getNelms() ];
		val3->getVal( dv3, val3->getNelms() );
		for ( unsigned i=0; i<val3->getNelms(); i++ ) {
			if ( dv3[i] != 8.5 ) {
				throw TestFailed("v3 readback");
			}
		}

			

		top->dumpConfigToYaml( cnfg );
{
YAML::Emitter e;
e << cnfg;
std::cout << e.c_str() << "\n";
}
		if ( 1 == pass ) {
			// 'val2' should not be present
			YAML::const_iterator it( cnfg[0]["mmio"].begin() );
			YAML::const_iterator ite( cnfg[0]["mmio"].end() );
			bool v1Found = false;
			bool v2Found = false;
			while ( it != ite ) {
				if ( (*it)["val1"] )
					v1Found = true;
				else if ( (*it)["val2"] )
					v2Found = true;
				++it;
			}
			YAML::Emitter e;
			e << cnfg;
			std::cout << e.c_str() << "\n";
			if ( ! v1Found )
				throw TestFailed("val1 not found in config dump");
			if ( v2Found )
				throw TestFailed("val2 still found in config dump");
		}
	}

		
} catch (TestFailed e) {
	std::cerr << "Test FAILED: " << e.msg_ << "\n";
	throw;
}

	if ( CpswObjCounter::report(true) ) {
		throw TestFailed("Unexpected object count");
	}

	rval = 0;

	return rval;
}
