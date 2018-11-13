 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_hub.h>
#include <cpsw_obj_cnt.h>

#include <cpsw_mem_dev.h>

#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <iostream>
#include <math.h>

extern void _setHostByteOrder(ByteOrder);

#define NELMS  10
#define STRIDE 9
#define BITS16 12
#define SHFT16 2

#define MEMSIZE  2048
#define MMIOSIZE 1024

class TestFailed {
public: const char *info;
	TestFailed(const char *info):info(info){}
};

class AioTester : public IAsyncIO {
volatile bool done_;
public:
	AioTester()
	: done_(false)
	{
	}

	virtual void callback(CPSWError *status)
	{
		if ( status )
			throw TestFailed(status->getInfo().c_str());
		done_ = true;
	}

	virtual void wait()
	{
		while ( ! done_ ) {
			sleep(1);
			printf("still Sleeping\n");
		}
		done_ = false;
	}
};

int64_t rrr()
{
	int64_t r = 0;
	for ( int i=0; i<8; i++ )
		r = (r<<8) ^ random();
	return r;
}

void swp(uint8_t* b, unsigned sz, ByteOrder nat)
{
	if ( nat != hostByteOrder() ) {
		for ( unsigned i=0; i<sz/2; i++ ) {
			uint8_t tmp = b[sz-1-i];
			b[sz-1-i] = b[i];
			b[i]      = tmp;
		}
	}
}

template <typename EL> void perr(ByteOrder mem, ByteOrder nat, bool s, int i, EL got, EL exp, int elsz, uint8_t *buf, int wlen, int writeback)
{
	swp( (uint8_t*)&got, sizeof(got), nat);
	printf("Mismatch (width %li, swap len %i, mem: %sE, host: %s, %ssigned, %s-writeback) @%i, got 0x%04" PRIx64 ", exp 0x%04" PRIx64 "\n",
			(long int)sizeof(EL),
			wlen,
			LE == mem ? "L":"B",
			nat == hostByteOrder() ? "NAT" : "INV",
			s ? "" : "un",
			writeback ? "post" : "pre",
			i, (uint64_t)got, (uint64_t)exp);
	for ( int j=0; j<elsz; j++ )
		printf("m[%i]: %01" PRIx8 " ", j, buf[i*STRIDE+j]);
	printf("\n");
}


template <typename EL> void tst(uint8_t *buf, unsigned bufsz, ScalVal_RO val, ByteOrder mbo, int shft, int wlen)
{
	int bo;
	ByteOrder native = hostByteOrder();
	unsigned int i;
	int got, j;
	EL ui[NELMS];
	uint64_t uo[NELMS];

	int64_t r;

	ScalVal val_w = cpsw::dynamic_pointer_cast<ScalVal::element_type, ScalVal_RO::element_type>(val);

	for ( bo = 0; bo < 2; bo++ ) {

		for ( i=0; i<bufsz; i++ )
			buf[i] = rrr();

		unsigned sizeBits = val->getSizeBits();
		uint64_t msk   = sizeBits >= 64 ? -1ULL : ((1ULL<<sizeBits) - 1);

		int sizeBytes  = (sizeBits + 7)/8;

		uint64_t bmsk  = sizeBytes >= 8 ? -1ULL : ((1ULL<<(8*sizeBytes)) - 1);
		uint64_t smsk  = bmsk << shft;

		unsigned wbits = 8*wlen;

		for ( i=0; i<NELMS; i++ ) {
			uo[i] = (rrr()>>(8*sizeof(r)-sizeBits));

			uint64_t v = uo[i];

			if ( wlen > 0 ) {
				uint64_t vout = 0;
				uint64_t vswp = v;
				for ( j = 0; j<(int)((sizeBits + 7)/wbits); j++ ) {
					vout = (vout<<wbits) | ( vswp & ((1ULL<<wbits)-1) );
					vswp >>= wbits;
				}

				v = (vout & bmsk) | (v & ~bmsk);
			}

			v = ((v<<shft) & smsk) | (rrr() & ~smsk);

			int jinc = LE == mbo ? +1 : -1;
			int jend = LE == mbo ? val->getSize() : -1;
			int jbeg = LE == mbo ?  0 : val->getSize()-1;
//printf("val[%i]: 0x%" PRIx64 "\n", i, v);

			for ( j=jbeg; j != jend; j+=jinc ) {
				buf[i*STRIDE + j] = v;
				v = v>>8;
			}
		}

		for ( int writeback = 0; 1; writeback++ ) {

			got = val->getVal(ui, sizeof(ui)/sizeof(ui[0]));

			if ( got != NELMS )
				throw TestFailed("unexpected number of elements read");

			for ( i=0; i<NELMS; i++ ) {
				EL v = uo[i];
				if ( ! val->isSigned() )
					v &= msk;
				swp((uint8_t*)&ui[i], sizeof(ui[0]), native);
				if ( v != ui[i] ) {
					perr<EL>(mbo, native, val->isSigned(), i, ui[i], v, val->getSize(), buf, wlen, writeback);
					throw TestFailed("mismatch (le, signed)");
				}
			}

	if ( writeback >= (val_w ? 1 : 0) )
		break;

			for ( i=0; i<bufsz; i++ )
				buf[i] = rrr();

			for ( i=0; i<NELMS; i++ ) {
				swp((uint8_t*)&ui[i], sizeof(ui[0]), native);
			}

			got = val_w->setVal(ui, sizeof(ui)/sizeof(ui[0]));

			for ( i=0; i<NELMS; i++ ) {
				ui[i] = rrr();
			}

			if ( got != NELMS )
				throw TestFailed("unexpected number of elements written");

		}

		_setHostByteOrder( BE == hostByteOrder() ? LE : BE );

	}

	_setHostByteOrder( native );
}

typedef shared_ptr<const MemDev::element_type> ConstMemDev;

int
main(int argc, char **argv)
{
const char *use_yaml = 0;
const char *dmp_yaml = 0;

int opt;

	while ( (opt = getopt(argc, argv, "y:Y:")) > 0 ) {
		switch (opt) {
			default:
			throw TestFailed("unknown commandline option");

			case 'y': dmp_yaml = optarg; break;
			case 'Y': use_yaml = optarg; break;
		}
	}

bool        use_hier = !!use_yaml;

try {

{
Hub       root;
uint8_t  *membuf;
unsigned  membufsz;

MMIODev mmio_le;
MMIODev mmio_be;

ConstMemDev cmm;
MemDev       mm;

int  bits[] = { 4, 13, 16, 22, 32, 44, 64 };
int  shft[] = { 0, 3, 4, 7 };
bool sign[] = { true, false };
unsigned bits_idx, shft_idx, sign_idx;

int run = !use_yaml ? 3 : 2;

while ( --run > 0 ) {

	if ( use_yaml ) {
		root = IPath::loadYamlFile(use_yaml, "root")->origin();

		cmm = cpsw::dynamic_pointer_cast<ConstMemDev::element_type>( root );

		if ( ! cmm ) {
			throw TestFailed("MemDev not found after loading YAML");
		}

	} else {
		if ( run > 1 ) {
			        mm      = IMemDev::create("mem",MEMSIZE);
			MMIODev mmio    = IMMIODev::create("mmio",MEMSIZE, UNKNOWN);
			mmio_le = IMMIODev::create("le", MMIOSIZE, LE);
			mmio_be = IMMIODev::create("be", MMIOSIZE, BE);

			mm->addAtAddress( mmio );
			mmio->addAtAddress( mmio_le,    0, 2);
			mmio->addAtAddress( mmio_be,    0, 2);

			root     = cmm = mm;
		} else {
			/* test cloning entire hierarchy */
			EntryImpl ei = CShObj::clone( mm->getSelf() );
			root = ei->isHub();
			cmm  = cpsw::dynamic_pointer_cast<ConstMemDev::element_type>( root );
			mmio_le.reset();
			mmio_be.reset();
			mm.reset();
			use_hier = true;
		}
	}

	membuf   = cmm->getBufp();
	membufsz = cmm->getSize();

	Path p_be( root->findByName("mmio/be[0]") );
	Path p_le( root->findByName("mmio/le[0]") );

	try {

		IIntField::Builder bldr = IIntField::IBuilder::create();

		bldr->mode( IVal_Base::RW );

		for ( bits_idx = 0; bits_idx < sizeof(bits)/sizeof(bits[0]); bits_idx++ ) {

			bldr->sizeBits( bits[bits_idx] );

			for ( shft_idx = 0; shft_idx < sizeof(shft)/sizeof(shft[0]); shft_idx++ ) {

				bldr->lsBit( shft[shft_idx] );

				for ( sign_idx = 0; sign_idx < sizeof(sign)/sizeof(sign[0]); sign_idx++ ) {
					char nm[100];

					bldr->isSigned( sign[sign_idx] );

					unsigned wswap;
					unsigned wswape = (bits[bits_idx] + 7)/8; 

					if ( wswape == 8  && shft[shft_idx] ) {
						/* test code uses int64 internally and shift overflows */
						wswape = 0;
					}

					for ( wswap = 0; wswap <= wswape; wswap++ ) {

						if ( wswap > 0 && ( (wswape % wswap) != 0 || wswap == wswape ) ) 
							continue;

						bldr->wordSwap( wswap );

						sprintf(nm,"i%i-%i-%c-%i", bits[bits_idx], shft[shft_idx], sign[sign_idx] ? 's' : 'u', wswap);

						if ( ! use_hier ) {
						IntField entry;
							try {
								entry = bldr->build( nm );
							} catch (InvalidArgError &e) {
								if ( wswap && (bits[bits_idx] & 7) ) {
									IIntField::Builder bldrRO = bldr->clone();

									bldrRO->mode( IVal_Base::RO );

									entry = bldrRO->build( nm );
								} else {
									throw;
								}
							}
							mmio_le->addAtAddress( entry, 0, NELMS, STRIDE );
							mmio_be->addAtAddress( entry, 0, NELMS, STRIDE );
printf("%s\n", entry->getName());
						}
else
printf("%s\n", nm);

						ScalVal_RO v_le, v_be;
						try {
							v_le = IScalVal::create( p_le->findByName( nm ) );
							v_be = IScalVal::create( p_be->findByName( nm ) );
v_be->getPath()->dump(stdout); std::cout << "\n";
						} catch ( InterfaceNotImplementedError &e ) {
							// cannot be writable if word-swap and bits % 8 != 0
							if ( wswap && (bits[bits_idx] & 7) ) {
								v_le = IScalVal_RO::create( p_le->findByName( nm ) );
								v_be = IScalVal_RO::create( p_be->findByName( nm ) );
							} else {
								throw;
							}
						}

						try {
							tst<uint8_t>( membuf, membufsz, v_le, LE, shft[shft_idx], wswap);
							tst<uint8_t>( membuf, membufsz, v_be, BE, shft[shft_idx], wswap);

							tst<uint16_t>( membuf, membufsz, v_le, LE, shft[shft_idx], wswap);
							tst<uint16_t>( membuf, membufsz, v_be, BE, shft[shft_idx], wswap);

							tst<uint32_t>( membuf, membufsz, v_le, LE, shft[shft_idx], wswap);
							tst<uint32_t>( membuf, membufsz, v_be, BE, shft[shft_idx], wswap);

							if ( bits[bits_idx] < 64 || shft[shft_idx] == 0 ) {
								/* test code uses int64 internally and shift overflows */
								tst<uint64_t>( membuf, membufsz, v_le, LE, shft[shft_idx], wswap);
								tst<uint64_t>( membuf, membufsz, v_be, BE, shft[shft_idx], wswap);
							}
						} catch ( CPSWError &e ) {
							printf("Error at %s\n", nm);
							throw;
						} catch ( TestFailed &e ) {
							printf("Test Failure (%s) at %s\n", e.info, nm);
							throw;
						}
					}
				}
			}
		}

		if ( dmp_yaml ) {
			IYamlSupport::dumpYamlFile( root, dmp_yaml, "root" );
		}

	} catch ( CPSWError &e ) {
		printf("Error: %s\n", e.getInfo().c_str());
		throw ;
	}

	uint16_t  vs = 0x8123;
	uint64_t  vl, vle;


	ScalVal v_le = IScalVal::create( p_le->findByName( "i32-4-s-2[0]" ) );
	ScalVal v_be = IScalVal::create( p_be->findByName( "i32-4-s-2[0]" ) );

	for (int i=0; i<9; i++ ) *(membuf+i)=0xaa;
	v_le->setVal( &vs, 1 );
	v_le->getVal( &vl, 1 );
	vle = (int64_t)(int16_t)vs;
	if ( vl != vle ) {
		printf("Readback of written value FAILED (%s) got %" PRIx64 " -- expected %" PRIx64 "\n", v_le->getName(), vl, vle);
		throw TestFailed("Readback failed");
	}

	for (int i=0; i<9; i++ )
		printf("%02x ", *(membuf+i));
	printf("\n***\n");

	for (int i=0; i<9; i++ ) *(membuf+i)=0xaa;
	v_be->setVal( &vs, 1 );
	v_be->getVal( &vl, 1 );
	vle = (int64_t)(int16_t)vs;
	if ( vl != vle ) {
		printf("Readback of written value FAILED (%s) got %" PRIx64 " -- expected %" PRIx64 "\n", v_le->getName(), vl, vle);
		throw TestFailed("Readback failed");
	}

	for (int i=0; i<9; i++ )
		printf("%02x ", *(membuf+i));
	printf("\n");

	// test array slicing

	ScalVal v_arr = IScalVal::create( p_le->findByName( "i32-0-u-0" ) );
	if ( v_arr->getNelms() != NELMS )
		throw TestFailed("expected number of elements mismatch");

	uint32_t buf[NELMS];
	

	v_arr->setVal( 0xa5a5a5a5 );

	v_arr->getVal( buf, NELMS );

	for ( int i=0; i<NELMS; i++ ) {
		if ( buf[i] != 0xa5a5a5a5 )
			throw TestFailed("clearing array with 'setVal' failed");
		buf[i] = i;
	}

	v_arr->setVal( buf, NELMS );

	memset(buf, 0x55, sizeof(buf) );

	IndexRange rng(0);

	unsigned steps[] = { 1, 2, NELMS };

	for ( unsigned steps_idx = 0; steps_idx < sizeof(steps)/sizeof(steps[0]); steps_idx++ ) {
		rng.setFromTo(0, steps[steps_idx]-1);
		for ( unsigned i=0; i<NELMS; i+= steps[steps_idx] ) {
			v_arr->getVal( buf, steps[steps_idx], &rng );
			for ( unsigned j=0; j < steps[steps_idx]; j++ ) {
				if ( buf[j] != (uint32_t)(i+j) ) {
					throw TestFailed("reading sliced array failed");
				}
			}
			++rng;
		}
	}

	for ( unsigned steps_idx = 0; steps_idx < sizeof(steps)/sizeof(steps[0]); steps_idx++ ) {
		rng.setFromTo(0, steps[steps_idx]-1);
		for ( unsigned i=0; i<NELMS; i+= steps[steps_idx] ) {
			v_arr->setVal( 0xa5a5a5a5, &rng );
			++rng;
		}
		v_arr->getVal( buf, NELMS );
		for ( unsigned i=0; i<NELMS; i++ ) {
			if ( buf[i] != 0xa5a5a5a5 ) {
				throw TestFailed("swiping with single-valued setVal failed");
			}
		}

		for ( unsigned i=0; i<NELMS; i++ )
			buf[i] = (uint32_t)i;

		rng.setFromTo(0, steps[steps_idx]-1);
		for ( unsigned i=0; i<NELMS; i+= steps[steps_idx] ) {
			v_arr->setVal( buf + i, steps[steps_idx], &rng );
			++rng;
		}

		memset(buf, 0 , sizeof(buf));
		v_arr->getVal(buf, NELMS);
		for ( unsigned i=0; i<NELMS; i++ ) {
			if ( buf[i] != (uint32_t) i ) {
				throw TestFailed("writing sliced array failed");
			}
		}
	}

	// test two-dimensional array
	std::cout << "2D-TEST\n";
	{

	ConstMemDevImpl memDev = cpsw::dynamic_pointer_cast<ConstMemDevImpl::element_type>( root );
		if ( ! memDev )
			throw TestFailed("Memory Device not found");
		uint8_t *bufp=memDev->getBufp();

		// test for shft == 0 (stride > size -> individual MMIO) and shft == 4 (stride == size == 9 -> chunked MMIO)
		for ( unsigned shft = 0; shft <= 4; shft += 4 ) {

			char nam[200];
			sprintf(nam, "mmio/le/i64-%d-u-0", shft);

			v_arr = IScalVal::create( root->findByName( nam ) );


			memset(bufp, 0, memDev->getSize());

			uint64_t v = 0;

			unsigned elsz = ((v_arr->getSizeBits() + shft) + 7) / 8;

			for ( unsigned off=0; off < MEMSIZE; off += MMIOSIZE) {

				for ( unsigned i=0; i<STRIDE*NELMS; i+= STRIDE ) {
					uint64_t vv = v << shft;
					for ( unsigned k = 0; k < elsz; k++ ) {
						bufp[off+i+k] = vv;
						vv >>= 8;
					}
					v++;
				}
			}


			unsigned nelms = v_arr->getNelms();

			if ( nelms != 2*NELMS )
				throw TestFailed("2-D Array size mismatch");

			uint64_t dat[ nelms ];

			v_arr->getVal(dat, nelms);

			for ( unsigned i=0; i < nelms; i++ ) {
				if ( dat[i] != i ) {
					char buf[200];
					snprintf(buf,sizeof(buf),"2-D Array readback mismatch (i=%d) -- got %" PRId64 "", i, dat[i]);
					throw TestFailed(buf);
				}
			}

			memset(dat, 0xee, sizeof(dat[0])*nelms);

			// test IndexRange

			IndexRange rng(2,3);
			v_arr->getVal(dat, nelms, &rng);
			if ( dat[0] != 2 || dat[1] != 3 || dat[2] != NELMS+2 || dat[3] != NELMS+3 )
				throw TestFailed("Testing IndexRange on 2D array failed");

			for ( unsigned i=0; i < nelms; i++ ) {
				dat[i] = ~(uint64_t)i;
			}

			v_arr->setVal(dat, nelms);

			v = 0;
			for ( unsigned off=0; off < MEMSIZE; off += MMIOSIZE) {
				for ( unsigned i=0; i<STRIDE*NELMS; i+= STRIDE ) {
					uint16_t vs = bufp[off+i+elsz-1];
					uint64_t vv = vs >> shft;
					for ( int  k = elsz - 2; k >= 0; k-- ) {
						vs = (vs<<8) | bufp[off+i+k];
						vv = (vv<<8) | ((vs >> shft) & 0xff);
					}
					if ( ~v != vv ) {
						char buf[200];
						snprintf(buf,sizeof(buf),"2-D Array write-check mismatch (v=%" PRId64 ") -- got 0x%" PRIx64 ", expected 0x%" PRIx64 , v, vv, ~v);
						throw TestFailed(buf);
					}
					v++;
				}
			}

		}

	}
	std::cout << "double-TEST\n";
	{
		char nam[200];
			sprintf(nam, "mmio/le/i16-0-u-0");

			v_arr = IScalVal::create( root->findByName( nam ) );
			DoubleVal d_arr = IDoubleVal::create( root->findByName( nam ) );

			unsigned nelms = v_arr->getNelms();
			unsigned got;

			uint16_t u16[nelms];
			double   dbl[nelms];

			for ( unsigned i=0; i<nelms; i++ )
				u16[i]=(uint16_t)(i-4);

			v_arr->setVal(u16, nelms);

			got = d_arr->getVal(dbl, nelms);

			if ( got != nelms )
				throw TestFailed("double readback -- got less elements than expected");

			for ( int i=0; i<(int)nelms; i++ ) {
				if ( fabs( dbl[i] - (double)( (i-4) < 0 ? 65536 + i - 4 : i - 4) ) > .000001 )
					throw TestFailed("unsigned <-> double mismatch");
			}

			sprintf(nam, "mmio/le/i16-0-s-0");
			d_arr = IDoubleVal::create( root->findByName( nam ) );

			memset(dbl, 0, sizeof(dbl));

			shared_ptr<AioTester> wai = cpsw::make_shared<AioTester>();

			got = d_arr->getVal(wai, dbl, nelms);
			if ( got != nelms )
				throw TestFailed("double readback -- got less elements than expected");
			wai->wait();

			for ( int i=0; i<(int)nelms; i++ ) {
				if ( fabs( dbl[i] - (double)(i-4)) > .000001 ) {
					printf("dbl[%d]: %g - expected %d\n", i, dbl[i], i-4);
					throw TestFailed("unsigned <-> double mismatch");
				}
			}
	}

}


}

	if ( CpswObjCounter::report(true) ) {
		throw TestFailed("Unexpected object count");
	}

} catch ( CPSWError e ) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
} catch ( TestFailed e ) {
	printf("Test failed -- because: %s\n", e.info);
	throw;
}

	printf("CPSW_SVAL test PASSED\n");
	return 0;

IndexRange r( 0,0 );
}
