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
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <getopt.h>

// Run this test program together with the 'udpsrv' program using matching IP address and port parameters

#include <udpsrv_regdefs.h>

#include <cpsw_obj_cnt.h>

using boost::dynamic_pointer_cast;

class TestFailed {};

class Clr {
private:
	ScalVal v;
	ScalVal vsbe, vsle;
public:
	Clr(Path where, MMIODev h)
	{
	char  nam[100];
		if ( h && ! where->verifyAtTail( IPath::create(h) ) ) {
			printf("Tail is %s; expected %s\n", where->tail()->getName(), h->getName());
			throw ConfigurationError("Clr: tail of 'where' path is not a Hub!");
		}
		sprintf(nam,"clr");
		if ( h ) // NULL if yaml loaded
		h->addAtAddress( IIntField::create(nam, 32), REG_CLR_OFF );
		v = IScalVal::create( where->findByName( nam ) );

		sprintf(nam,"scratch-be");
		if ( h ) // NULL if yaml loaded
		h->addAtAddress( IIntField::create(nam, 64), REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, BE );
		vsbe = IScalVal::create( where->findByName( nam ) );

		sprintf(nam,"scratch-le");
		if ( h ) // NULL if yaml loaded
		h->addAtAddress( IIntField::create(nam, 64), REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, LE );
		vsle = IScalVal::create( where->findByName( nam ) );
	}

	uint64_t getScratch(ByteOrder bo)
	{
	uint64_t rval;
		if ( BE == bo ) {
			vsbe->getVal( &rval, 1 );
		} else {
			vsle->getVal( &rval, 1 );
		}
		if ( bo != hostByteOrder() && 0 ) {
#ifdef __GNUC__
			rval = __builtin_bswap64( rval );
#else
#error      "64bit byte-swap needs to be implemented"
#endif
		}
		return rval;
	}

	uint64_t getPatt(uint32_t val, ByteOrder bo)
	{
		if ( val < 2 )
			return (val ? 0xffffffffffffffffUL : 0x000000000000UL );
		if ( (val == 2) != (bo == BE) )
			return 0xefcdab8967452301;
		else
			return 0x0123456789abcdef;
	}

	void clr(uint32_t val)
	{
	uint64_t v64, vexp;
		if ( 3 == val ) {
			vsle->setVal( 0x0123456789abcdef );
		} else if ( 2 == val ) {
			vsbe->setVal( 0x0123456789abcdef );
		} else {
			v->setVal( &val, 1 );
		}

		vexp = getPatt( val, BE );

		vsbe->getVal( &v64, 1 );
		if ( v64 != vexp ) {
			fprintf(stderr,"Unable to %s pseudo registers (got 0x%016"PRIx64" -- expected 0x%016"PRIx64"\n", val ? "set" : "clear", v64, vexp);
			throw TestFailed();
		}
	}

};


int
main(int argc, char **argv)
{
const char *ip_addr  = "127.0.0.1";
int        *i_p;
int         vers     = 2;
INetIODev::ProtocolVersion protoVers;
int         port     = 8192;
unsigned sizes[]     = { 1, 4, 8, 12, 16, 22, 30, 32, 45, 64 };
unsigned lsbs[]      = { 0, 3 };
unsigned offs[]      = { 0, 1, 2, 3, 4, 5 };
unsigned vc          = 0;
int      useRssi     = 0;
int      tDest       = -1;
unsigned byteResHack =  0;
const char *use_yaml =  0;
const char *dmp_yaml =  0;

	for ( int opt; (opt = getopt(argc, argv, "a:V:p:rt:bY:y:")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr     = optarg;   break;
			case 'V': i_p         = &vers;    break;
			case 'p': i_p         = &port;    break;
			case 'r': useRssi     = 1;        break;
			case 't': i_p         = &tDest;   break;
			case 'b': byteResHack = 0x10000;  break;
			case 'Y': use_yaml    = optarg;   break;
			case 'y': dmp_yaml    = optarg;   break;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( vers != 1 && vers != 2 && vers != 3 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1..3\n", vers);
		throw TestFailed();
	}

	if ( vers < 3 )
		byteResHack = 0;

	{
	Hub root;

	try {
		MMIODev  srvm;

		if ( use_yaml ) {
			root = IPath::loadYamlFile( use_yaml, "root" )->origin();
		} else {
		NetIODev netio = INetIODev::create("fpga", ip_addr);
		MMIODev   mmio = IMMIODev::create ("mmio",0x100000);
		          srvm = IMMIODev::create ("srvm",0x10000, LE);

		root = netio;

		ProtoStackBuilder pbldr( IProtoStackBuilder::create() );

		mmio->addAtAddress( srvm, REGBASE );

		switch ( vers ) {
			case 1: protoVers = IProtoStackBuilder::SRP_UDP_V1; break;
			case 2: protoVers = IProtoStackBuilder::SRP_UDP_V2; break;
			case 3: protoVers = IProtoStackBuilder::SRP_UDP_V3; break;
			default:
				throw TestFailed();
		}

		pbldr->setSRPVersion              (             protoVers );
		pbldr->setUdpPort                 (                  port );
		pbldr->setSRPTimeoutUS            (               1000000 );
		pbldr->setSRPRetryCount           (       byteResHack | 4 ); // enable byte-resolution access
		pbldr->setSRPMuxVirtualChannel    (                    vc );
		pbldr->useRssi                    (               useRssi );
		if ( tDest >= 0 ) {
			pbldr->setTDestMuxTDEST       (                 tDest );
		}


		netio->addAtAddress( mmio, pbldr );

		}

		Path pre = root->findByName("mmio/srvm");

		uint64_t v_expect = 0x0123456789abcdefUL;
		uint64_t v_got, v;
		unsigned size_idx, lsb_idx, off_idx;
		char nam[100];
		for ( size_idx = 0; size_idx < sizeof(sizes)/sizeof(sizes[0]); size_idx++ ) {
			for ( lsb_idx = 0; lsb_idx < sizeof(lsbs)/sizeof(lsbs[0]); lsb_idx++ ) {
				for ( off_idx = 0; off_idx < sizeof(offs)/sizeof(offs[0]); off_idx++ ) {
					if ( offs[off_idx] + (sizes[size_idx] + lsbs[lsb_idx] + 7)/8 > 8 )
						continue;
					sprintf(nam,"vro-%i-%i-%i-be",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);
					if ( ! use_yaml )
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx], IIntField::RO), offs[off_idx]    , 1, IMMIODev::STRIDE_AUTO, BE ); 
					ScalVal_RO v_be = IScalVal_RO::create( pre->findByName(nam) );
					sprintf(nam,"vro-%i-%i-%i-le",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);
					if ( ! use_yaml )
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx], IIntField::RO), offs[off_idx] + 8, 1, IMMIODev::STRIDE_AUTO, LE ); 
					ScalVal_RO v_le = IScalVal_RO::create( pre->findByName(nam) );

					v_le->getVal( &v_got, 1 );
					uint64_t shft_le = (offs[off_idx]*8 + lsbs[lsb_idx]);
					uint64_t msk     = sizes[size_idx] > 63 ? 0xffffffffffffffffULL : ((((uint64_t)1)<<sizes[size_idx]) - 1 );
					if ( v_got != (v = ((v_expect >> shft_le) & msk)) ) {
						fprintf(stderr,"Mismatch (%s - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_le->getName(), v_le->getSizeBits(), sizes[size_idx], v_got, v);
						throw TestFailed();
					}

					v_be->getVal( &v_got, 1 );
					uint64_t shft_be = ((sizeof(v_expect) - (offs[off_idx] + v_be->getSize()))*8 + lsbs[lsb_idx]);
					if ( v_got != (v = ((v_expect >> shft_be) & msk)) ) {
						fprintf(stderr,"Mismatch (%s - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_be->getName(), v_be->getSizeBits(), sizes[size_idx], v_got, v);
{
uint64_t xxx;
	v_be->getVal( &xxx );
}
						throw TestFailed();
					}
				}
			}
		}

		Clr clr( pre, srvm );

		clr.clr( 0 );
		clr.clr( 1 );
		clr.clr( 2 );
		clr.clr( 3 );

		for ( size_idx = 0; size_idx < sizeof(sizes)/sizeof(sizes[0]); size_idx++ ) {
			for ( lsb_idx = 0; lsb_idx < sizeof(lsbs)/sizeof(lsbs[0]); lsb_idx++ ) {
				for ( off_idx = 0; off_idx < sizeof(offs)/sizeof(offs[0]); off_idx++ ) {
					if ( offs[off_idx] + (sizes[size_idx] + lsbs[lsb_idx] + 7)/8 > 8 )
						continue;
					sprintf(nam,"vrw-%i-%i-%i-be",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);

					if ( ! use_yaml )
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx] + REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, BE ); 

					ScalVal v_be = IScalVal::create( pre->findByName(nam) );
					sprintf(nam,"vrw-%i-%i-%i-le",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);

					if ( ! use_yaml )
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx] + REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, LE ); 

					ScalVal v_le = IScalVal::create( pre->findByName(nam) );

					uint64_t shft_le = (offs[off_idx]*8 + lsbs[lsb_idx]);
					uint64_t shft_be = ((sizeof(v_expect) - (offs[off_idx] + v_be->getSize()))*8 + lsbs[lsb_idx]);
					uint64_t msk     = sizes[size_idx] > 63 ? 0xffffffffffffffffULL : ((((uint64_t)1)<<sizes[size_idx]) - 1 );
					uint64_t msk_le  = msk << shft_le;
					uint64_t msk_be  = msk << shft_be;

					for (int patt=0; patt < 4; patt++ ) {
						clr.clr( patt );
						v_le->setVal( &v_expect, 1 );
						v_got = clr.getScratch( LE );

						v = (clr.getPatt(patt, LE) & ~msk_le) | ((v_expect << shft_le) & msk_le);

						if ( v_got != v ) {
							fprintf(stderr,"Mismatch (%s patt %i - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_le->getName(), patt, v_le->getSizeBits(), sizes[size_idx], v_got, v);
							throw TestFailed();
						}

						clr.clr( patt );
						v_be->setVal( &v_expect, 1 );
						v_got = clr.getScratch( BE );

						v = (clr.getPatt(patt, BE) & ~msk_be) | ((v_expect << shft_be) & msk_be);

						if ( v_got != v ) {
							fprintf(stderr,"Mismatch (%s patt %i - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_be->getName(), patt, v_be->getSizeBits(), sizes[size_idx], v_got, v);
							throw TestFailed();
						}
					}
				}
			}
		}

		sprintf(nam,"arr-16-0-2-le");
		if ( ! use_yaml )
		srvm->addAtAddress( IIntField::create(nam, 16, false, 0), REG_ARR_OFF + 2, (REG_ARR_SZ-4)/2, IMMIODev::STRIDE_AUTO, LE ); 
		ScalVal v_le = IScalVal::create( pre->findByName(nam) );
		sprintf(nam,"arr-32-0-0-le");
		if ( ! use_yaml )
		srvm->addAtAddress( IIntField::create(nam, 32, false, 0), REG_ARR_OFF + 0, (REG_ARR_SZ)/4, IMMIODev::STRIDE_AUTO, LE ); 
		ScalVal v32_le = IScalVal::create( pre->findByName(nam) );

		int n,i;

		n = v_le->getNelms();
		uint16_t v16[n];
		v_le->getVal(v16,n);

		if ( v16[0] != 0xddcc ) {
			fprintf(stderr, "array16 val mismatch @i==%i\n (got %x -- expected %x)\n", 0, v16[0], 0xddcc);
			throw TestFailed();
		}
		for ( i=1; i<n-1; i++ ) {
			if ( v16[i] != i+1 ) {
				fprintf(stderr, "array16 val mismatch @i==%i\n (got %x)\n", i, v16[i]);
				throw TestFailed();
			}
		}
		if ( v16[i] != 0xfbfa ) {
			fprintf(stderr, "array16 val mismatch @i==%i\n (got %x -- expected %x)\n", i, v16[i], 0xfbfa);
			throw TestFailed();
		}
		memset(v16, 0x55, sizeof(v16[0])*n);
		v_le->setVal(v16,n);

		int n32 = v32_le->getNelms();
		uint32_t v32[n32];
		v32_le->getVal(v32, n32);
		if ( v32[0] != 0x5555bbaa ) {
			i = 0;
			fprintf(stderr, "array32 val mismatch @i==%i\n (got %x -- expected %x)\n", i, v32[i], 0x5555bbaa);
			throw TestFailed();
		}
		for ( i=1; i<n32-1; i++ ) {
			if ( v32[i] != 0x55555555 ) {
				fprintf(stderr, "array32 val mismatch @i==%i\n (got %x -- expected %x)\n", i, v32[i], 0x55555555);
				throw TestFailed();
			}
		}
		if ( v32[i] != 0xfdfc5555 ) {
			fprintf(stderr, "array32 val mismatch @i==%i\n (got %x -- expected %x)\n", i, v32[i], 0xfdfc5555);
			throw TestFailed();
		}

		root->findByName("mmio")->tail()->dump( stdout );

		if ( dmp_yaml ) {
			IYamlSupport::dumpYamlFile( root, dmp_yaml, "root" );
		}

	} catch (IOError &e) {
		if ( root )
			root->findByName("mmio")->tail()->dump( stdout );
		printf("I/O Error -- is 'udpsrv' running (with matching protocol version) ?\n");
		printf("             note: udpsrv debugging messages might slow it down...\n");
		throw;
	} catch (CPSWError &e) {
		printf("CPSW Error: %s\n", e.getInfo().c_str());
		throw;
	}

	}

	if ( CpswObjCounter::report() ) {
		printf("Leaked Objects!\n");
		throw TestFailed();
	}

	printf("SRP - UDP test PASSED\n");

	return 0;
}
