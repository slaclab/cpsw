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
#include <cpsw_mutex.h>
#include <cpsw_condvar.h>

class TestFailed {};

typedef uint16_t TYPE;

class CAIO;
typedef shared_ptr<CAIO> AIO;

class CAIO : public CMtx, public CCond, public IAsyncIO {
private:
	bool done_;
public:
	CAIO()
	: done_(false)
	{
	}

	virtual void callback(CPSWError *err)
	{
		if ( err ) {
			fprintf(stderr,"AIO callback with error: %s\n", err->getInfo().c_str());
			throw TestFailed();
		}
		done_ = true;
		pthread_cond_signal( CCond::getp() );
	}

	virtual void wait()
	{
	CMtx::lg guard( this );
		while ( ! done_ ) {
			pthread_cond_wait( CCond::getp(), CMtx::getp() );
		}
	}
};

static void check(ScalVal arr, TYPE patt, bool async)
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

		if ( async ) {
			AIO aio = AIO( new CAIO() );
			arr->getVal( aio, buf, nelms );
			aio->wait();
		} else {
			arr->getVal(buf, nelms);
		}

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
		fprintf(stderr,"of pattern 0x%04" PRIx16 " ", patt);
	}
	fprintf(stderr,"failed @i %d\n", i);
	throw TestFailed();
}

#define DO_V3

int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
unsigned    nelms   = REG_ARR_SZ / sizeof(TYPE);
int         rval    = 1;
int         depack2 = 0;

unsigned    vers    = 3;
unsigned    tdest   = 1000; /* off */
unsigned    port    = 0;
unsigned   *u_p;
int         opt;

IProtoStackBuilder::SRPProtoVersion pvers;

	while ( (opt = getopt(argc, argv, "V:p:t:2h")) > 0 ) {
		u_p = 0;
		switch ( opt ) {
			case 'V': u_p = &vers;  break;
			case 'p': u_p = &port;  break;
			case 't': u_p = &tdest; break;
			case '2': depack2 = 1;  break;
			case 'h':
				rval = 0; /* fall thru */
			default:
				fprintf(stderr,"usage: %s [-V <srp_version> ] [-p <port> ] [-t <tdest> ] [-2] [-h]\n", argv[0]);
				return rval;
		}
		if ( u_p && (1 != sscanf(optarg, "%i", u_p)) ) {
			fprintf(stderr,"ERROR: Unable to scan value for option '-%c'\n", opt);
			return 1;
		}
	}

	if ( depack2 ) {
		if ( 0 == port ) port = 8204;
	}

	switch ( vers ) {
		case 3:
			if ( 0   == port  ) port  = 8200;
			if ( 255 <  tdest ) tdest = 1;
			pvers = IProtoStackBuilder::SRP_UDP_V3;
			break;
		case 2:
			if ( 0   == port  ) port  = 8202;
			pvers = IProtoStackBuilder::SRP_UDP_V2;
			break;
		default:
			fprintf(stderr,"ERROR: invalid/unsupported SRP protocol version %d\n", vers);
			return 1;
	}

	{
	NetIODev root;
	try {
		{
		        root   = INetIODev::create("netio", ip_addr);
		MMIODev mmio   = IMMIODev::create ("mmio",  MEM_SIZE);
		MMIODev srvm   = IMMIODev::create ("srvm",  REG_ARR_SZ, LE);
		IntField   f   = IIntField::create("data",  8*sizeof(TYPE), false, 0);
		srvm->addAtAddress( f, 0, nelms );
		
		mmio->addAtAddress( srvm, REGBASE+REG_ARR_OFF );

		ProtoStackBuilder pbldr( IProtoStackBuilder::create() );

		pbldr->setSRPVersion(                 pvers );
		pbldr->setUdpPort   (                  port );
		pbldr->useRssi      (                  true );
		if ( tdest > 255 ) {
			pbldr->useTDestMux  (                 false );
		} else {
			pbldr->setTDestMuxTDEST(             tdest );
		}
		if ( depack2 ) {
			pbldr->setDepackVersion( IProtoStackBuilder::DEPACKETIZER_V2 );
		}
	
		root->addAtAddress( mmio, pbldr );
		}

		ScalVal arr = IScalVal::create( root->findByName("mmio/srvm/data") );
		if ( arr->getNelms() != nelms ) {
			fprintf(stderr, "SRP V%d # elements mismatch\n", vers);
			throw TestFailed();
		}

		check(arr, 0xdead, false);
		check(arr,      0, false);
		check(arr, 0xdead, true );
		check(arr,      0, true );

		
	} catch ( CPSWError &e ) {
		fprintf(stderr,"CPSW Error caught: %s\n", e.getInfo().c_str());
		throw e;
	}
	}
	if ( CpswObjCounter::report(stderr) ) {
		printf("Leaked Objects!\n");
		throw TestFailed();
	}
	printf("Test PASSED\n");
	return 0;
}
