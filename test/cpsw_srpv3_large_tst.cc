 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <string.h>
#include <stdio.h>

#include <udpsrv_regdefs.h>

#include <cpsw_obj_cnt.h>

class TestFailed {};

typedef uint16_t TYPE;

static void check(ScalVal arr, TYPE patt)
{
unsigned    nelms = arr->getNelms();
TYPE        buf[nelms];
unsigned    i;

		if ( patt ) {
			arr->setVal(patt);
		} else {
			for ( i=0; i<nelms; i++ )
				buf[i] = i;
			arr->setVal( buf, nelms );
		}
		
		memset(buf, ~patt, nelms*sizeof(buf[0]));

		arr->getVal(buf, nelms);

		if ( patt ) {
			for ( i=0; i<nelms; i++ ) {
				if ( buf[i] != patt ) {
					goto failed;
				}
			}
		} else {
			for ( i=0; i<nelms; i++ ) {
				if ( buf[i] != i ) {
					goto failed;
				}
			}
		}
	return;
failed:
	fprintf(stderr,"Readback ");
	if ( patt ) {
		fprintf(stderr,"of pattern 0x%04"PRIx16" ", patt);
	}
	fprintf(stderr,"failed @i %d\n", i);
	throw TestFailed();
}

int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
int         v2port  = 8202;
int         v3port  = 8200;
int         v3tdst  = 1;
unsigned    nelms   = REG_ARR_SZ / sizeof(TYPE);


	{
	NetIODev root;
	try {
		        root   = INetIODev::create("netio", ip_addr);
		MMIODev mmiov2 = IMMIODev::create("mmiov2",MEM_SIZE);
		MMIODev mmiov3 = IMMIODev::create("mmiov3",MEM_SIZE);
		MMIODev srvm   = IMMIODev::create("srvm",  REG_ARR_SZ, LE);
		IntField   f   = IIntField::create("data", 8*sizeof(TYPE), false, 0);
		srvm->addAtAddress( f, 0, nelms );
		
		mmiov2->addAtAddress( srvm, REGBASE+REG_ARR_OFF );
		mmiov3->addAtAddress( srvm, REGBASE+REG_ARR_OFF );

		INetIODev::PortBuilder pbldr( root->createPortBuilder() );
		pbldr->setSRPVersion( INetIODev::SRP_UDP_V2 );
		pbldr->setUdpPort   (                v2port );
		pbldr->useRssi      (                  true );
		pbldr->useTDestMux  (                 false );

		root->addAtAddress( mmiov2, pbldr );

		ScalVal arrv2 = IScalVal::create( root->findByName("mmiov2/srvm/data") );
		if ( arrv2->getNelms() != nelms ) {
			fprintf(stderr, "V2 # elements mismatch\n");
			throw TestFailed();
		}

		pbldr->setSRPVersion( INetIODev::SRP_UDP_V3 );
		pbldr->setSRPTimeoutUS(             1000000 );
		pbldr->setUdpPort   (                v3port );
		pbldr->setTDestMuxTDEST(             v3tdst );
		pbldr->useTDestMux  (                  true );

		root->addAtAddress( mmiov3, pbldr );

		ScalVal arrv3 = IScalVal::create( root->findByName("mmiov3/srvm/data") );

	
		if ( arrv3->getNelms() != nelms ) {
			fprintf(stderr, "V3 # elements mismatch\n");
			throw TestFailed();
		}

		check(arrv2, 0xdead);
		check(arrv3,      0);

		
	} catch ( CPSWError e ) {
		fprintf(stderr,"CPSW Error caught: %s\n", e.getInfo().c_str());
		throw e;
	}
	}
	if ( CpswObjCounter::report() ) {
		printf("Leaked Objects!\n");
		throw TestFailed();
	}
	return 0;
}
