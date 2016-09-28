 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <udpsrv_regdefs.h>
#include <vector>
#include <string>
#include <dlfcn.h>

#define USE_FOR_TESTPROGRAM_ONLY
#include <cpsw_myCommand.h>

#include <stdio.h>

class TestFailed {};


// For the 'non-yaml' test case we normally would just link
// 'myCommand' into the application.
//
// The test program, however, should also exercise the dynamic
// loading facility of the YAML-factory. Therefore we don't
// link myCommand. This means that we need some run-time linking
// code here:

#define SHOBJ_NAME "MyCommand.so"

static IMyCommandImpl_create linkMyCommand()
{
void *dlh;
	if ( ! (dlh = dlopen(SHOBJ_NAME, RTLD_LAZY | RTLD_GLOBAL)) ) {
		fprintf(stderr,"Unable to load '%s' : %s\n", SHOBJ_NAME, dlerror());
		return 0;
	}

IMyCommandImpl_create rval = (IMyCommandImpl_create) dlsym( dlh, IMYCOMMANDIMPL_CREATE );

	if ( ! rval ) {
		fprintf(stderr,"Unable to find '%s' in '%s'\n", IMYCOMMANDIMPL_CREATE, SHOBJ_NAME);
	}
	return rval;
}


int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
int         opt;
const char *use_yaml = 0;
const char *dmp_yaml = 0;

IMyCommandImpl_create iMyCommandImpl_create = 0;

	while ( (opt = getopt(argc, argv, "y:Y:")) > 0 ) {
		switch (opt) {
			default:
				fprintf(stderr,"Unknown option -'%c'\n", opt);
				throw TestFailed();
			case 'Y': use_yaml    = optarg;   break;
			case 'y': dmp_yaml    = optarg;   break;
		}
	}

try {

Hub root;

	if ( use_yaml ) {
		root = IPath::loadYamlFile( use_yaml, "root" )->origin();
	} else {

		if ( ! (iMyCommandImpl_create = linkMyCommand()) ) {
			fprintf(stderr,"Unable to link '%s'\n", SHOBJ_NAME);
			throw TestFailed();
		}

	NetIODev netio( INetIODev::create("root", ip_addr ) );
	MMIODev  mmio ( IMMIODev::create( "mmio", MEM_SIZE) );
	Dev      dummy_container( IDev::create( "dummy", 0 ) );

		mmio->addAtAddress( IIntField::create("val" , 32     ), REGBASE+REG_SCR_OFF );
		mmio->addAtAddress( iMyCommandImpl_create("cmd", "val"), 0 );

	ISequenceCommand::Items items;
		items.push_back( ISequenceCommand::Item( "../val", 0 ) );
		items.push_back( ISequenceCommand::Item( "usleep", 100000 ) );
		items.push_back( ISequenceCommand::Item( "../cmd", 0 ) );

		dummy_container->addAtAddress( ISequenceCommand::create("seq", &items) );

		mmio->addAtAddress( dummy_container, 0 );


		netio->addAtAddress( mmio, INetIODev::createPortBuilder() );

		root = netio;
	}

	if ( dmp_yaml ) {
		IYamlSupport::dumpYamlFile( root, dmp_yaml, "root" );
	}

	ScalVal val( IScalVal::create( root->findByName("mmio/val") ) );

	Command cmd( ICommand::create( root->findByName("mmio/cmd") ) );

	Command seq( ICommand::create( root->findByName("mmio/dummy/seq") ) );

	uint64_t v = -1ULL;

	val->setVal( (uint64_t)0 );
	val->getVal( &v, 1 );
	if ( v != 0 ) {
		throw TestFailed();
	}

	cmd->execute();

	val->getVal( &v, 1 );
	if ( v != 0xdeadbeef ) {
		throw TestFailed();
	}

	val->setVal( 44 );
	val->getVal( &v, 1 );
	if ( v != 44 ) {
		throw TestFailed();
	}


	struct timespec then;
	clock_gettime( CLOCK_MONOTONIC, &then );

	seq->execute();

	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );

	val->getVal( &v, 1 );
	if ( v != 0xdeadbeef ) {
		throw TestFailed();
	}

	if ( now.tv_nsec < then.tv_nsec ) {
		now.tv_nsec += 1000000000;
		now.tv_sec  -= 1;
	}

	uint64_t difft = (now.tv_nsec - then.tv_nsec)/1000;

	difft += (now.tv_sec - then.tv_sec) * 1000000;

	if ( difft < 100000 ) {
		throw TestFailed();
	}


} catch (CPSWError e) {
	fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
	throw e;
}

	fprintf(stderr,"Command test PASSED\n");
	return 0;
}
