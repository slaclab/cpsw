#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>

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
	char nam[100];
		if ( ! h )
			throw ConfigurationError("Clr: tail of 'where' path is not a Hub!");
		sprintf(nam,"clr");
		h->addAtAddress( IIntField::create(nam, 32), REG_CLR_OFF );
		v = IScalVal::create( where->findByName( nam ) );

		sprintf(nam,"scratch-be");
		h->addAtAddress( IIntField::create(nam, 64), REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, BE );
		vsbe = IScalVal::create( where->findByName( nam ) );

		sprintf(nam,"scratch-le");
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

	void clr(uint32_t val)
	{
	uint64_t v64;
		v->setVal( &val, 1 );

		vsbe->getVal( &v64, 1 );
		if ( v64 != (val ? 0xffffffffffffffffUL : 0x000000000000UL ) ) {
			fprintf(stderr,"Unable to %s pseudo registers\n", val ? "set" : "clear");
			throw TestFailed();
		}
	}

};


int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
int        *i_p;
int         vers    = 2;
int         port    = 8192;
unsigned sizes[]    = { 1, 4, 8, 12, 16, 22, 32, 45, 64 };
unsigned lsbs[]     = { 0, 3 };
unsigned offs[]     = { 0, 1, 2, 3, 4, 5 };
unsigned vc         = 0;
int      useRssi    = 0;
int      tDest      = -1;

	for ( int opt; (opt = getopt(argc, argv, "a:V:p:r")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr = optarg;   break;
			case 'V': i_p     = &vers;    break;
			case 'p': i_p     = &port;    break;
			case 'r': useRssi = 1;        break;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( vers != 1 && vers != 2 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1 or 2\n", vers);
		throw TestFailed();
	}

	try {

		NoSsiDev  root = INoSsiDev::create("fpga", ip_addr);
		MMIODev   mmio = IMMIODev::create ("mmio",0x100000);
		MMIODev   srvm = IMMIODev::create ("srvm",0x10000, LE);

		mmio->addAtAddress( srvm, REGBASE );

		root->addAtAddress( mmio, 1 == vers ? INoSsiDev::SRP_UDP_V1 : INoSsiDev::SRP_UDP_V2, (unsigned short)port, 1000000, 4, vc, useRssi, tDest );

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
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx]    , 1, IMMIODev::STRIDE_AUTO, BE ); 
					ScalVal_RO v_be = IScalVal_RO::create( pre->findByName(nam) );
					sprintf(nam,"vro-%i-%i-%i-le",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx] + 8, 1, IMMIODev::STRIDE_AUTO, LE ); 
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
						throw TestFailed();
					}
				}
			}
		}

		Clr clr( pre, srvm );

		clr.clr( 0 );
		clr.clr( 1 );

		for ( size_idx = 0; size_idx < sizeof(sizes)/sizeof(sizes[0]); size_idx++ ) {
			for ( lsb_idx = 0; lsb_idx < sizeof(lsbs)/sizeof(lsbs[0]); lsb_idx++ ) {
				for ( off_idx = 0; off_idx < sizeof(offs)/sizeof(offs[0]); off_idx++ ) {
					if ( offs[off_idx] + (sizes[size_idx] + lsbs[lsb_idx] + 7)/8 > 8 )
						continue;
					sprintf(nam,"vrw-%i-%i-%i-be",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx] + REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, BE ); 
					ScalVal v_be = IScalVal::create( pre->findByName(nam) );
					sprintf(nam,"vrw-%i-%i-%i-le",sizes[size_idx], lsbs[lsb_idx], offs[off_idx]);
					srvm->addAtAddress( IIntField::create(nam, sizes[size_idx], false, lsbs[lsb_idx]), offs[off_idx] + REG_SCR_OFF, 1, IMMIODev::STRIDE_AUTO, LE ); 
					ScalVal v_le = IScalVal::create( pre->findByName(nam) );

					uint64_t shft_le = (offs[off_idx]*8 + lsbs[lsb_idx]);
					uint64_t shft_be = ((sizeof(v_expect) - (offs[off_idx] + v_be->getSize()))*8 + lsbs[lsb_idx]);
					uint64_t msk     = sizes[size_idx] > 63 ? 0xffffffffffffffffULL : ((((uint64_t)1)<<sizes[size_idx]) - 1 );
					uint64_t msk_le  = msk << shft_le;
					uint64_t msk_be  = msk << shft_be;

					for (int patt=0; patt < 2; patt++ ) {
						clr.clr( patt );
						v_le->setVal( &v_expect, 1 );
						v_got = clr.getScratch( LE );

						v = ((patt ? -1UL : 0UL) & ~msk_le) | ((v_expect << shft_le) & msk_le);

						if ( v_got != v ) {
							fprintf(stderr,"Mismatch (%s patt %i - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_le->getName(), patt, v_le->getSizeBits(), sizes[size_idx], v_got, v);
							throw TestFailed();
						}

						clr.clr( patt );
						v_be->setVal( &v_expect, 1 );
						v_got = clr.getScratch( BE );

						v = ((patt ? -1UL : 0UL) & ~msk_be) | ((v_expect << shft_be) & msk_be);

						if ( v_got != v ) {
							fprintf(stderr,"Mismatch (%s patt %i - %"PRIu64" - %u): got %"PRIx64" - expected %"PRIx64"\n", v_be->getName(), patt, v_be->getSizeBits(), sizes[size_idx], v_got, v);
							throw TestFailed();
						}
					}
				}
			}
		}

		sprintf(nam,"arr-16-0-2-le");
		srvm->addAtAddress( IIntField::create(nam, 16, false, 0), REG_ARR_OFF + 2, (REG_ARR_SZ-4)/2, IMMIODev::STRIDE_AUTO, LE ); 
		ScalVal v_le = IScalVal::create( pre->findByName(nam) );
		sprintf(nam,"arr-32-0-0-le");
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

	} catch (IOError &e) {
		printf("I/O Error -- is 'udpsrv' running (with matching protocol version) ?\n");
		printf("             note: udpsrv debugging messages might slow it down...\n");
		throw;
	} catch (CPSWError &e) {
		printf("CPSW Error: %s\n", e.getInfo().c_str());
		throw;
	}

	if ( CpswObjCounter::report() ) {
		printf("Leaked Objects!\n");
		throw TestFailed();
	}

	printf("SRP - UDP test PASSED\n");

	return 0;
}
