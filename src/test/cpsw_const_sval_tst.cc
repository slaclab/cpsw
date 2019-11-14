 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <cpsw_obj_cnt.h>

#include <cpsw_yaml_keydefs.h>

#include <stdio.h>

static const char *yaml=
"#schemaversion 3.0.0\n"
"root:\n"
"  " YAML_KEY_class ": MemDev\n"
"  " YAML_KEY_size ":  0x2000\n"
"  " YAML_KEY_children ":\n"
"    mmio:\n"
"      " YAML_KEY_class ": MMIODev\n"
"      " YAML_KEY_size ":  0x2000\n"
"      " YAML_KEY_at ": {}\n"
"      " YAML_KEY_children ":\n"
"        cStr:\n"
"          " YAML_KEY_class ": ConstIntField\n"
"          " YAML_KEY_value ": \"Hello I'm there\"\n"
"          " YAML_KEY_encoding ": ASCII\n"
"          " YAML_KEY_at ": {" YAML_KEY_offset ": 0, nelms: 10}\n"
"        cDbl:\n"
"          " YAML_KEY_class ": ConstIntField\n"
"          " YAML_KEY_value ": 3.5890\n"
"          " YAML_KEY_encoding ": IEEE_754\n"
"          " YAML_KEY_at ": {" YAML_KEY_offset ": 0}\n"
"        cInt:\n"
"          " YAML_KEY_class ": ConstIntField\n"
"          " YAML_KEY_value ": 0xdeadbeef\n"
"          " YAML_KEY_at ": {" YAML_KEY_offset ": 0}\n"
;

class TestFailed {};

int
main(int argc, char **argv)
{
{
	Path root = IPath::loadYamlStream( yaml );
	Path dblp = root->findByName( "mmio/cDbl" );
	Path strp = root->findByName( "mmio/cStr" );
	Path intp = root->findByName( "mmio/cInt" );
	try {
		IScalVal_RO::create( dblp );
		// shouldn't be able to create scalVal
		throw TestFailed();
	} catch ( InterfaceNotImplementedError )
	{
	}

	DoubleVal_RO dv = IDoubleVal_RO::create( dblp );
	ScalVal_RO   sv = IScalVal_RO::create( strp );
	ScalVal_RO   iv = IScalVal_RO::create( intp );

	double d;
	dv->getVal( &d, 1 );
	if ( d != 3.589 )
		throw TestFailed();
	CString st[10];
	sv->getVal( st, sizeof(st)/sizeof(st[0]) );

	unsigned n;
	for ( n = 0; n<sizeof(st)/sizeof(st[0]); n++ ) {
		if ( ::strcmp( st[n]->c_str(), "Hello I'm there" ) )
			throw TestFailed();
	}
	uint32_t i;
	iv->getVal( &i, 1 );
	if ( i != 0xdeadbeef )
	{
		throw TestFailed();
	}

	
}
	if ( CpswObjCounter::report(stderr, true) ) {
		throw TestFailed();
	}

	printf("%s PASSED\n", argv[0]);
	return 0;
}
