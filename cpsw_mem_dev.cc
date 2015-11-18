#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

CMemDevImpl::CMemDevImpl(FKey k, uint64_t size)
: CDevImpl(k, size), buf( new uint8_t[size] )
{
}

CMemDevImpl::~CMemDevImpl()
{
	delete [] buf;
}

void CMemDevImpl::addAtAddress(Field child, unsigned nelms)
{
IAddress::AKey k = getAKey();

	add( make_shared<CMemDevAddressImpl>(k, nelms), child );
}

CMemDevAddressImpl::CMemDevAddressImpl(AKey k, unsigned nelms)
: CAddressImpl(k, nelms)
{
}

uint64_t CMemDevAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
int toget = dbytes < sbytes ? dbytes : sbytes;
	if ( off + toget > owner->getSize() ) {
//printf("off %lu, dbytes %lu, size %lu\n", off, dbytes, owner->getSize());
		throw ConfigurationError("MemDevAddress: read out of range");
	}
	memcpy(dst, owner->getBufp() + off, toget);
//printf("MemDev read from off %lli", off);
//for ( int ii=0; ii<dbytes; ii++) printf(" 0x%02x", dst[ii]); printf("\n");
	return toget;
}

uint64_t CMemDevAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const 
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
uint8_t *buf  = owner->getBufp();
unsigned put  = dbytes < sbytes ? dbytes : sbytes;
unsigned rval = put;

	if ( off + put > owner->getSize() ) {
		throw ConfigurationError("MemDevAddress: write out of range");
	}

	if ( (msk1 || mskn) && put == 1 ) {
		msk1 |= mskn;
		mskn  = 0;
	}

	if ( msk1 ) {
		/* merge 1st byte */
		buf[off] = ( (buf[off]) & msk1 ) | ( src[0] & ~msk1 ) ;
		off++;
		put--;
		src++;
	}

	if ( mskn ) {
		put--;
	}
	if ( put ) {
		memcpy(owner->getBufp() + off, src, put);
	}
	if ( mskn ) {
		/* merge last byte */
		buf[off+put] = (buf[off+put] & mskn ) | ( src[put] & ~mskn );
	}
	return rval;
}

MemDev IMemDev::create(const char *name, uint64_t size)
{
	return CEntryImpl::create<CMemDevImpl>(name, size);
}
