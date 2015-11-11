#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
shared_ptr<MemDevImpl> owner( getOwner()->getSelfAs< shared_ptr<MemDevImpl> >() );
	if ( off + dbytes > owner->getSize() ) {
//printf("off %lu, dbytes %lu, size %lu\n", off, dbytes, owner->getSize());
		throw ConfigurationError("MemDevAddress: read out of range");
	}
	memcpy(dst, owner->getBufp() + off, dbytes);
//printf("MemDev read from off %lli", off);
//for ( int ii=0; ii<dbytes; ii++) printf(" 0x%02x", dst[ii]); printf("\n");
	return dbytes;
}

MemDev IMemDev::create(const char *name, uint64_t size)
{
	return EntryImpl::create<MemDevImpl>(name, size);
}
