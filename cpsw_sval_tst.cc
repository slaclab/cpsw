#include <cpsw_api_builder.h>
#include <cpsw_hub.h>
#include <cpsw_obj_cnt.h>

#include <stdio.h>
#include <stdlib.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <iostream>

extern void _setHostByteOrder(ByteOrder);

#define NELMS  10
#define STRIDE 9
#define BITS16 12
#define SHFT16 2

class TestFailed {
public:
	const char *info;
	TestFailed(const char *info):info(info){}
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

template <typename EL> void perr(ByteOrder mem, ByteOrder nat, bool s, int i, EL got, EL exp, int elsz, MemDev m, int wlen, int writeback)
{
uint8_t *buf = m->getBufp();
	swp( (uint8_t*)&got, sizeof(got), nat);
	printf("Mismatch (width %li, swap len %i, mem: %sE, host: %s, %ssigned, %s-writeback) @%i, got 0x%04"PRIx64", exp 0x%04"PRIx64"\n",
			(long int)sizeof(EL),
			wlen,
			LE == mem ? "L":"B",
			nat == hostByteOrder() ? "NAT" : "INV",
			s ? "" : "un",
			writeback ? "post" : "pre",
			i, (uint64_t)got, (uint64_t)exp);
	for ( int j=0; j<elsz; j++ )
		printf("m[%i]: %01"PRIx8" ", j, buf[i*STRIDE+j]);
	printf("\n");
}


template <typename EL> void tst(MemDev mmp, ScalVal_RO val, ByteOrder mbo, int shft, int wlen)
{
	uint8_t *buf = mmp->getBufp();
	int bo;
	ByteOrder native = hostByteOrder();
	unsigned int i;
	int got, j;
	EL ui[NELMS];
	uint64_t uo[NELMS];

	int64_t r;

	ScalVal val_w = boost::dynamic_pointer_cast<ScalVal::element_type, ScalVal_RO::element_type>(val);

	for ( bo = 0; bo < 2; bo++ ) {

		for ( i=0; i<mmp->getSize(); i++ )
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
//printf("val[%i]: 0x%"PRIx64"\n", i, v);

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
					perr<EL>(mbo, native, val->isSigned(), i, ui[i], v, val->getSize(), mmp, wlen, writeback);
					throw TestFailed("mismatch (le, signed)");
				}
			}

	if ( writeback >= (val_w ? 1 : 0) )
		break;

			for ( i=0; i<mmp->getSize(); i++ )
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

using boost::dynamic_pointer_cast;

int
main()
{

try {

{
MemDev  mm      = IMemDev::create("mem",2048);
MMIODev mmio    = IMMIODev::create("mmio",2048, UNKNOWN);
MMIODev mmio_le = IMMIODev::create("le", 1024, LE);
MMIODev mmio_be = IMMIODev::create("be", 1024, BE);
Path p_be, p_le;

int  bits[] = { 4, 13, 16, 22, 32, 44, 64 };
int  shft[] = { 0, 3, 4, 7 };
bool sign[] = { true, false };

unsigned bits_idx, shft_idx, sign_idx;

	mm->addAtAddress( mmio );
	mmio->addAtAddress( mmio_le,    0);
	mmio->addAtAddress( mmio_be,    0);

	p_be = mm->findByName("mmio/be");
	p_le = mm->findByName("mmio/le");

	try {

		IIntField::Builder bldr = IIntField::IBuilder::create();

		bldr->mode( IIntField::RW );

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

						IntField e = bldr->build( nm );

						mmio_le->addAtAddress( e, 0, NELMS, STRIDE );
						mmio_be->addAtAddress( e, 0, NELMS, STRIDE );
printf("%s\n", e->getName());

						ScalVal_RO v_le, v_be;
						try {
							v_le = IScalVal::create( p_le->findByName( nm ) );
							v_be = IScalVal::create( p_be->findByName( nm ) );
v_be->getPath()->dump(stdout); std::cout << "\n";
						} catch ( InvalidArgError &e ) {
							// cannot be writable if word-swap and bits % 8 != 0
							if ( wswap && (bits[bits_idx] & 7) ) {
								v_le = IScalVal_RO::create( p_le->findByName( nm ) );
								v_be = IScalVal_RO::create( p_be->findByName( nm ) );
							} else {
								throw;
							}
						}

						try {
							tst<uint8_t>( mm, v_le, LE, shft[shft_idx], wswap);
							tst<uint8_t>( mm, v_be, BE, shft[shft_idx], wswap);

							tst<uint16_t>( mm, v_le, LE, shft[shft_idx], wswap);
							tst<uint16_t>( mm, v_be, BE, shft[shft_idx], wswap);

							tst<uint32_t>( mm, v_le, LE, shft[shft_idx], wswap);
							tst<uint32_t>( mm, v_be, BE, shft[shft_idx], wswap);

							if ( bits[bits_idx] < 64 || shft[shft_idx] == 0 ) {
								/* test code uses int64 internally and shift overflows */
								tst<uint64_t>( mm, v_le, LE, shft[shft_idx], wswap);
								tst<uint64_t>( mm, v_be, BE, shft[shft_idx], wswap);
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

	} catch ( CPSWError &e ) {
		printf("Error: %s\n", e.getInfo().c_str());
		throw ;
	}

	uint16_t  vs = 0x8123;
	uint64_t  vl, vle;


	ScalVal v_le = IScalVal::create( p_le->findByName( "i32-4-s-2[0]" ) );
	ScalVal v_be = IScalVal::create( p_be->findByName( "i32-4-s-2[0]" ) );

	for (int i=0; i<9; i++ ) *(mm->getBufp()+i)=0xaa;
	v_le->setVal( &vs, 1 );
	v_le->getVal( &vl, 1 );
	vle = (int64_t)(int16_t)vs;
	if ( vl != vle ) {
		printf("Readback of written value FAILED (%s) got %"PRIx64" -- expected %"PRIx64"\n", v_le->getName(), vl, vle);
		throw TestFailed("Readback failed");
	}

	for (int i=0; i<9; i++ )
		printf("%02x ", *(mm->getBufp()+i));
	printf("\n***\n");

	for (int i=0; i<9; i++ ) *(mm->getBufp()+i)=0xaa;
	v_be->setVal( &vs, 1 );
	v_be->getVal( &vl, 1 );
	vle = (int64_t)(int16_t)vs;
	if ( vl != vle ) {
		printf("Readback of written value FAILED (%s) got %"PRIx64" -- expected %"PRIx64"\n", v_le->getName(), vl, vle);
		throw TestFailed("Readback failed");
	}

	for (int i=0; i<9; i++ )
		printf("%02x ", *(mm->getBufp()+i));
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
