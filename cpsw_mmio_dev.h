#ifndef CPSW_MMIO_DEV_H
#define CPSW_MMIO_DEV_H

#include <cpsw_hub.h>
#include <cpsw_path.h>
#include <stdint.h>
#include <stdio.h>

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

		virtual uint64_t getOffset() const   { return offset_; }
		virtual uint64_t getStride() const   { return stride_; }

		// ANY subclass must implement clone(AKey) !
		virtual CMMIOAddressImpl *clone(AKey k) { return new CMMIOAddressImpl( *this ); }
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
		CMMIODevImpl(CMMIODevImpl &orig, Key &k)
		:CDevImpl(orig,k),
		 byteOrder_(orig.byteOrder_)
		{
		}

	public:
		CMMIODevImpl(Key &k, const char *name, uint64_t size, ByteOrder byteOrder) : CDevImpl(k, name, size), byteOrder_(byteOrder)
		{
		}

		CMMIODevImpl(Key &k, const YAML::Node &n);
		virtual void addAtAddress(Field child, const YAML::Node &node);

		virtual void dumpYamlPart(YAML::Node &) const;

		static  const char *_getClassName()       { return "MMIODev";       }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual CMMIODevImpl *clone(Key &k) { return new CMMIODevImpl( *this, k ); }

		virtual ByteOrder getByteOrder() const { return byteOrder_; }

		virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO, ByteOrder byteOrder = UNKNOWN);

};

#endif
