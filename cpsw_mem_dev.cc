#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef shared_ptr<MemDevImpl> MemDevImplP;

MemDevImpl::MemDevImpl(FKey k, uint64_t size)
: DevImpl(k, size), buf( new uint8_t[size] )
{
}

MemDevImpl::~MemDevImpl()
{
	delete [] buf;
}

void MemDevImpl::addAtAddress(Field child, unsigned nelms)
{
AKey k = getAKey();

	add( make_shared<MemDevAddressImpl>(k, nelms), child );
}

MemDevAddressImpl::MemDevAddressImpl(AKey k, unsigned nelms) : AddressImpl(k, nelms) {}

uint64_t MemDevAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
MemDevImplP owner( getOwnerAs<MemDevImplP>() );
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

uint64_t MemDevAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const 
{
MemDevImplP owner( getOwnerAs<MemDevImplP>() );
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
	return EntryImpl::create<MemDevImpl>(name, size);
}
