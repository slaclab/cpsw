#include <api_user.h>
#include <cpsw_hub.h>
#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MemDev::MemDev(const char *name, uint64_t size) : Dev(name, size)
{
	buf = new uint8_t[size];
}

MemDev::~MemDev()
{
	delete [] buf;
}

void MemDev::addAtAddr(Entry *child, unsigned nelms)
{
	Address *a = new MemDevAddress(this, nelms);
	add(a, child);
}

MemDevAddress::MemDevAddress(MemDev *owner, unsigned nelms) : Address(owner, nelms) {}

uint64_t MemDevAddress::read(CompositePathIterator *node, Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
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
