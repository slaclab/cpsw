#include <cpsw_api_builder.h>
#include <string.h>
#include <stdio.h>

#include <udpsrv_regdefs.h>

#include <cpsw_obj_cnt.h>

class TestFailed {};

typedef uint16_t TYPE;

int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
int         v2port  = 8202;
int         v3port  = 8200;
int         v3tdst  = 1;
unsigned    nelms   = REG_ARR_SZ / sizeof(TYPE);
unsigned    i;


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

		TYPE        buf[nelms];
		
		for ( i=0; i<nelms; i++ )
			buf[i] = i;

		unsigned chunk = nelms;
		unsigned rem;

		IndexRange rng(0,chunk-1);

		rem = nelms;
		i   = 0;

		while ( rem > chunk ) {
			arrv2->setVal( buf+i, chunk, &rng );
			rem -= chunk;
			++rng;
			i+=chunk;
		}

		if ( rem > 0 ) {
			rng.setTo(rng.getFrom() + rem-1);
			arrv2->setVal( buf+i, rem, &rng );
		}

		memset(buf, 0, nelms*sizeof(buf[0]));

		rng.setFromTo(0, nelms-1);
		arrv3->getVal(buf, nelms, &rng );
for ( i=0; i<=rng.getTo(); i++ ) printf("%d\n",buf[i]);

		
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
