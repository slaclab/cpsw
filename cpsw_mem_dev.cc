#include <cpsw_mem_dev.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cpsw_yaml.h>

CMemDevImpl::CMemDevImpl(Key &k, const char *name, uint64_t size)
: CDevImpl(k, name, size), buf_( new uint8_t[size] )
{
}

CMemDevImpl::CMemDevImpl(Key &key, YamlState &node)
: CDevImpl(key, node),
  buf_( new uint8_t[size_] )
{
	if ( 0 == size_ ) {
		throw InvalidArgError("'size' zero or unset");
	}
}


CMemDevImpl::CMemDevImpl(CMemDevImpl &orig, Key &k)
: CDevImpl(orig, k),
  buf_( new uint8_t[orig.getSize()] )
{
	memcpy( buf_, orig.buf_, orig.getSize() );
}

CMemDevImpl::~CMemDevImpl()
{
	delete [] buf_;
}

CMemDevImpl & CMemDevImpl::operator=(CMemDevImpl &orig)
{
	delete [] buf_;
	*this = orig;
	buf_ = new uint8_t[orig.getSize()];
	memcpy( buf_, orig.buf_, orig.getSize() );
	return *this;
}

void CMemDevImpl::addAtAddress(Field child, unsigned nelms)
{
IAddress::AKey k = getAKey();

	add( make_shared<CMemAddressImpl>(k, nelms), child );
}

CMemAddressImpl::CMemAddressImpl(AKey k, unsigned nelms)
: CAddressImpl(k, nelms)
{
}

uint64_t CMemAddressImpl::read(CompositePathIterator *node, CReadArgs *args) const
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
int toget = args->nbytes_;
	if ( args->off_ + toget > owner->getSize() ) {
//printf("off %lu, dbytes %lu, size %lu\n", off, dbytes, owner->getSize());
		throw ConfigurationError("MemAddress: read out of range");
	}
	memcpy(args->dst_, owner->getBufp() + args->off_, toget);
//printf("MemDev read from off %lli", off);
//for ( int ii=0; ii<dbytes; ii++) printf(" 0x%02x", dst[ii]); printf("\n");
	return toget;
}

uint64_t CMemAddressImpl::write(CompositePathIterator *node, CWriteArgs *args) const
{
MemDevImpl owner( getOwnerAs<MemDevImpl>() );
uint8_t *buf  = owner->getBufp();
unsigned put  = args->nbytes_;
unsigned rval = put;
uint8_t  msk1 = args->msk1_;
uint8_t  mskn = args->mskn_;
uint64_t off  = args->off_;
uint8_t *src  = args->src_;

	if ( off + put > owner->getSize() ) {
		throw ConfigurationError("MemAddress: write out of range");
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
	return CShObj::create<MemDevImpl>(name, size);
}
