 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_MMIO_DEV_H
#define CPSW_MMIO_DEV_H

#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <stdint.h>
#include <stdio.h>
#include <cpsw_yaml.h>

class CMMIODevImpl;
typedef shared_ptr<CMMIODevImpl> MMIODevImpl;

class CMMIOAddressImpl : public CAddressImpl {
	private:
		uint64_t offset_;    // in bytes
		uint64_t stride_;    // in bytes

	public:
		CMMIOAddressImpl(
			AKey     key,
			uint64_t offset   = 0,
			unsigned nelms    = 1,
			uint64_t stride   = IMMIODev::STRIDE_AUTO,
			ByteOrder byteOrder = UNKNOWN );

		CMMIOAddressImpl(AKey key, YamlState *ypath);

		CMMIOAddressImpl(const CMMIOAddressImpl &orig, AKey k)
		: CAddressImpl(orig, k),
		  offset_(orig.offset_),
		  stride_(orig.stride_)
		{
		}

		virtual uint64_t getOffset() const   { return offset_; }
		virtual uint64_t getStride() const   { return stride_; }

		// ANY subclass must implement clone(AKey) !
		virtual CMMIOAddressImpl *clone(AKey k) { return new CMMIOAddressImpl( *this, k ); }
		virtual void dump(FILE *f) const;
		virtual uint64_t  read(CompositePathIterator *node, CReadArgs  *args) const;
		virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;
		virtual void attach(EntryImpl child);
		virtual void dumpYamlPart(YAML::Node &) const;
};


class CMMIODevImpl : public CDevImpl, public virtual IMMIODev {
	private:
		ByteOrder byteOrder_;

	protected:
		CMMIODevImpl(const CMMIODevImpl &orig, Key &k)
		: CDevImpl(orig,k),
		  byteOrder_(orig.byteOrder_)
		{
		}

	public:
		CMMIODevImpl(Key &k, const char *name, uint64_t size, ByteOrder byteOrder)
		: CDevImpl(k, name, size),
		  byteOrder_(byteOrder)
		{
		}

		CMMIODevImpl(Key &k, YamlState *ypath);

		virtual void addAtAddress(Field child, YamlState *ypath);

		virtual void dumpYamlPart(YAML::Node &) const;

		static  const char *_getClassName()       { return "MMIODev";       }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual CMMIODevImpl *clone(Key &k)
		{
			return new CMMIODevImpl( *this, k );
		}

		virtual ByteOrder getByteOrder() const { return byteOrder_; }

		virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO, ByteOrder byteOrder = UNKNOWN);

};

#endif
