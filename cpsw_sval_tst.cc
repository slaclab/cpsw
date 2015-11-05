#include <api_user.h>
#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <cpsw_mmio_dev.h>
#include <stdlib.h>
#include <math.h>

class MemDev;

extern void _setHostByteOrder(ByteOrder);

class MemDevAddress : public Address {
	protected:
		MemDevAddress(MemDev *owner, unsigned nelms = 1);

		friend class MemDev;

	public:
		virtual uint64_t read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
};

class MemDev : public Dev {
	public:
		uint8_t *buf;

		MemDev(const char *name, uint64_t size) : Dev(name, size)
		{
			buf = new uint8_t[size];
		}

		virtual ~MemDev()
		{
			delete [] buf;
		}

		virtual void addAtAddr(Entry *child, unsigned nelms = 1) {
			Address *a = new MemDevAddress(this, nelms);
			add(a, child);
		}
};

MemDevAddress::MemDevAddress(MemDev *owner, unsigned nelms) : Address(owner, nelms) {}

uint64_t MemDevAddress::read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
const MemDev *owner = static_cast<const MemDev*>(getOwner());
	if ( off + dbytes > owner->getSize() ) {
//printf("off %lu, dbytes %lu, size %lu\n", off, dbytes, owner->getSize());
		throw ConfigurationError("MemDevAddress: read out of range");
	}
	memcpy(dst, owner->buf + off, dbytes);
//printf("MemDev read from off %lli", off);
//for ( int ii=0; ii<dbytes; ii++) printf(" 0x%02x", dst[ii]); printf("\n");
	return dbytes;
}

#define NELMS  10
#define STRIDE 9
#define BITS16 12
#define SHFT16 2

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

template <typename EL> void perr(ByteOrder mem, ByteOrder nat, bool s, int i, EL got, EL exp, int elsz, MemDev *m)
{
	swp( (uint8_t*)&got, sizeof(got), nat);
	printf("Mismatch (width %li, mem: %sE, host: %s, %ssigned) @%i, got 0x%04"PRIx64", exp 0x%04"PRIx64"\n",
			sizeof(EL),
			LE == mem ? "L":"B",
			nat == hostByteOrder() ? "NAT" : "INV",
			s ? "" : "un",
			i, (uint64_t)got, (uint64_t)exp);
	for ( int j=0; j<elsz; j++ )
		printf("m[%i]: %01"PRIx8" ", j, m->buf[i*STRIDE+j]);
	printf("\n");
}


template <typename EL> void tst(MemDev *mmp, ScalVal_RO val, ByteOrder mbo, int shft)
{
	int bo;
	ByteOrder native = hostByteOrder();
	int i, got, j;
	EL ui[NELMS];
	uint64_t uo[NELMS];

	int64_t r;

	for ( bo = 0; bo < 2; bo++ ) {

		for ( i=0; i<mmp->getSize(); i++ )
			mmp->buf[i] = rrr();

		uint64_t msk = val->getSizeBits() >= 64 ? -1ULL : ((1ULL<<val->getSizeBits()) - 1);
		uint64_t smsk = msk << shft;

		for ( i=0; i<NELMS; i++ ) {
			uo[i] = (rrr()>>(8*sizeof(r)-val->getSizeBits()));

			uint64_t v = ((uo[i]<<shft) & smsk) | (rrr() & ~smsk);

			int jinc = LE == mbo ? +1 : -1;
			int jend = LE == mbo ? val->getSize() : -1;
			int jbeg = LE == mbo ?  0 : val->getSize()-1;
//printf("val[%i]: 0x%"PRIx64"\n", i, v);

			for ( j=jbeg; j != jend; j+=jinc ) {
				mmp->buf[i*STRIDE + j] = v;
				v = v>>8;
			}
		}

		got = val->getVal(ui, sizeof(ui)/sizeof(ui[0]));

		if ( got != NELMS )
			throw CPSWError("unexpected number of elements read");

		for ( i=0; i<NELMS; i++ ) {
			EL v = uo[i];
			if ( ! val->isSigned() )
				v &= msk;
			swp((uint8_t*)&ui[i], sizeof(ui[0]), native);
			if ( v != ui[i] ) {
				perr<EL>(mbo, native, val->isSigned(), i, ui[i], v, val->getSize(), mmp);
				throw CPSWError("mismatch (le, signed)");
			}
		}

		_setHostByteOrder( BE == hostByteOrder() ? LE : BE );

	}

	_setHostByteOrder( native );
}

int
main()
{
MemDev  mm("mem",2048);
MMIODev mmio("mmio",2048, 0, UNKNOWN);
MMIODev mmio_le("le", 1024, 0, LE);
MMIODev mmio_be("be", 1024, 0, BE);
Path p_be, p_le;

int  bits[] = { 4, 13, 16, 22, 32, 44, 64 };
int  shft[] = { 0, 3, 7 };
bool sign[] = { true, false };

unsigned bits_idx, shft_idx, sign_idx;

	mm.addAtAddr( &mmio );
	mmio.addAtAddr( &mmio_le,    0);
	mmio.addAtAddr( &mmio_be,    0);

	p_be = mm.findByName("mmio/be");
	p_le = mm.findByName("mmio/le");

#if 1
	try {

		for ( bits_idx = 0; bits_idx < sizeof(bits)/sizeof(bits[0]); bits_idx++ ) {
			for ( shft_idx = 0; shft_idx < sizeof(shft)/sizeof(shft[0]); shft_idx++ ) {
				for ( sign_idx = 0; sign_idx < sizeof(sign)/sizeof(sign[0]); sign_idx++ ) {
					char nm[100];
					sprintf(nm,"i%i-%i-%c", bits[bits_idx], shft[shft_idx], sign[sign_idx] ? 's' : 'u');
					IntEntry *e = new IntEntry(nm, bits[bits_idx], sign[sign_idx], shft[shft_idx]);

					mmio_le.addAtAddr( e, 0, NELMS, STRIDE );
					mmio_be.addAtAddr( e, 0, NELMS, STRIDE );

					ScalVal_RO v_le = IScalVal_RO::create( p_le->findByName( nm ) );
					ScalVal_RO v_be = IScalVal_RO::create( p_be->findByName( nm ) );

					try {
						tst<uint8_t>( &mm, v_le, LE, shft[shft_idx] );
						tst<uint8_t>( &mm, v_be, BE, shft[shft_idx] );

						tst<uint16_t>( &mm, v_le, LE, shft[shft_idx] );
						tst<uint16_t>( &mm, v_be, BE, shft[shft_idx] );

						tst<uint32_t>( &mm, v_le, LE, shft[shft_idx] );
						tst<uint32_t>( &mm, v_be, BE, shft[shft_idx] );

						if ( bits[bits_idx] < 64 || shft[shft_idx] == 0 ) {
							/* test code uses int64 internally and shift overflows */
							tst<uint64_t>( &mm, v_le, LE, shft[shft_idx] );
							tst<uint64_t>( &mm, v_be, BE, shft[shft_idx] );
						}
					} catch ( CPSWError &e ) {
						printf("Error at %s\n", nm);
						throw;
					}
				}
			}
		}

	} catch ( CPSWError &e ) {
		printf("Error: %s\n", e.getInfo().c_str());
		throw ;
	}

#else
int got, i, bo;
ByteOrder native = hostByteOrder();

uint16_t ui16[NELMS], uo16[NELMS];
IntEntry e12_2_s("i12-2-s", BITS16, true,  SHFT16);
IntEntry e12_2_u("i12-2-u", BITS16, false, SHFT16);
IntEntry e12_0_s("i12-0-s", BITS16, true,  0);
IntEntry e12_0_u("i12-0-u", BITS16, false, 0);


	mmio_le.addAtAddr( &e12_2_s, 0, NELMS, STRIDE );
	mmio_le.addAtAddr( &e12_2_u, 0, NELMS, STRIDE );
	mmio_be.addAtAddr( &e12_2_s, 0, NELMS, STRIDE );
	mmio_be.addAtAddr( &e12_2_u, 0, NELMS, STRIDE );

	mmio_le.addAtAddr( &e12_0_s, 0, NELMS, STRIDE );
	mmio_le.addAtAddr( &e12_0_u, 0, NELMS, STRIDE );
	mmio_be.addAtAddr( &e12_0_s, 0, NELMS, STRIDE );
	mmio_be.addAtAddr( &e12_0_u, 0, NELMS, STRIDE );

	try {

		ScalVal_RO v_le_12_2_s = IScalVal_RO::create( mm.findByName("mmio/le/i12-2-s") );
		ScalVal_RO v_le_12_2_u = IScalVal_RO::create( mm.findByName("mmio/le/i12-2-u") );
		ScalVal_RO v_be_12_2_s = IScalVal_RO::create( mm.findByName("mmio/be/i12-2-s") );
		ScalVal_RO v_be_12_2_u = IScalVal_RO::create( mm.findByName("mmio/be/i12-2-u") );

		ScalVal_RO v_le_12_0_s = IScalVal_RO::create( mm.findByName("mmio/le/i12-0-s") );
		ScalVal_RO v_le_12_0_u = IScalVal_RO::create( mm.findByName("mmio/le/i12-0-u") );
		ScalVal_RO v_be_12_0_s = IScalVal_RO::create( mm.findByName("mmio/be/i12-0-s") );
		ScalVal_RO v_be_12_0_u = IScalVal_RO::create( mm.findByName("mmio/be/i12-0-u") );

		tst<uint16_t>( &mm, v_le_12_2_s, LE, SHFT16 );
		tst<uint16_t>( &mm, v_le_12_2_u, LE, SHFT16 );
		tst<uint16_t>( &mm, v_be_12_2_s, BE, SHFT16 );
		tst<uint16_t>( &mm, v_be_12_2_u, BE, SHFT16 );

		tst<uint32_t>( &mm, v_le_12_2_s, LE, SHFT16 );
		tst<uint32_t>( &mm, v_le_12_2_u, LE, SHFT16 );
		tst<uint32_t>( &mm, v_be_12_2_s, BE, SHFT16 );
		tst<uint32_t>( &mm, v_be_12_2_u, BE, SHFT16 );

		tst<uint64_t>( &mm, v_le_12_2_s, LE, SHFT16 );
		tst<uint64_t>( &mm, v_le_12_2_u, LE, SHFT16 );
		tst<uint64_t>( &mm, v_be_12_2_s, BE, SHFT16 );
		tst<uint64_t>( &mm, v_be_12_2_u, BE, SHFT16 );

		tst<uint8_t>( &mm, v_le_12_2_s, LE, SHFT16 );
		tst<uint8_t>( &mm, v_le_12_2_u, LE, SHFT16 );
		tst<uint8_t>( &mm, v_be_12_2_s, BE, SHFT16 );
		tst<uint8_t>( &mm, v_be_12_2_u, BE, SHFT16 );

		tst<uint16_t>( &mm, v_le_12_0_s, LE, 0 );
		tst<uint16_t>( &mm, v_le_12_0_u, LE, 0 );
		tst<uint16_t>( &mm, v_be_12_0_s, BE, 0 );
		tst<uint16_t>( &mm, v_be_12_0_u, BE, 0 );

		tst<uint32_t>( &mm, v_le_12_0_s, LE, 0 );
		tst<uint32_t>( &mm, v_le_12_0_u, LE, 0 );
		tst<uint32_t>( &mm, v_be_12_0_s, BE, 0 );
		tst<uint32_t>( &mm, v_be_12_0_u, BE, 0 );

		tst<uint64_t>( &mm, v_le_12_0_s, LE, 0 );
		tst<uint64_t>( &mm, v_le_12_0_u, LE, 0 );
		tst<uint64_t>( &mm, v_be_12_0_s, BE, 0 );
		tst<uint64_t>( &mm, v_be_12_0_u, BE, 0 );

		tst<uint8_t>( &mm, v_le_12_0_s, LE, 0 );
		tst<uint8_t>( &mm, v_le_12_0_u, LE, 0 );
		tst<uint8_t>( &mm, v_be_12_0_s, BE, 0 );
		tst<uint8_t>( &mm, v_be_12_0_u, BE, 0 );

	} catch ( CPSWError &e ) {
		printf("Error: %s\n", e.getInfo().c_str());
		throw ;
	}
#endif

	printf("CPSW_SVAL test PASSED\n");

	return 0;
}
