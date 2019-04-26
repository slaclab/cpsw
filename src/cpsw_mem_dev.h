 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_MEM_TST_DEV_H
#define CPSW_MEM_TST_DEV_H

#include <cpsw_hub.h>
#include <string.h>
#include <cpsw_yaml.h>

class CMemDevImpl;
typedef shared_ptr<CMemDevImpl>            MemDevImpl;
typedef shared_ptr<const CMemDevImpl> ConstMemDevImpl;

// pseudo-device with memory backing (for testing)

class CMemAddressImpl : public CAddressImpl {
	private:
		int align_;
		uint64_t (*read_func_ )(uint8_t *dst, uint8_t *src, size_t n);
		uint64_t (*write_func_)(uint8_t *dst, uint8_t *src, size_t n, uint8_t msk1, uint8_t mskn);

		void checkAlign();

	public:
		CMemAddressImpl(AKey key, int align);
		CMemAddressImpl(AKey key, YamlState &ypath);

		CMemAddressImpl(const CMemAddressImpl &orig, AKey k)
		: CAddressImpl( orig,           k),
		  align_      ( orig.align_      ),
		  read_func_  ( orig.read_func_  ),
		  write_func_ ( orig.write_func_ )
		{
		}

		virtual int getAlignment() const
		{
			return align_;
		}

		// ANY subclass must implement clone(AKey) !
		virtual CMemAddressImpl *clone(AKey k) { return new CMemAddressImpl( *this, k ); }

		virtual uint64_t read(CReadArgs *args)   const;
		virtual uint64_t write(CWriteArgs *args) const;
};

class CMemDevImpl : public CDevImpl, public virtual IMemDev {
	private:
		uint8_t    *buf_;
		bool        isExt_;
        std::string fileName_;

	protected:
		CMemDevImpl(const CMemDevImpl &orig, Key &k);

	public:
		CMemDevImpl(Key &k, const char *name, uint64_t size, uint8_t *ext_buf);

		CMemDevImpl(Key &k, YamlState &n);

		static  const char *_getClassName()       { return "MemDev";        }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual void dumpYamlPart(YAML::Node &node) const;

		virtual void
		addAtAddress(Field child, YamlState &ypath)
		{
			doAddAtAddress<CMemAddressImpl>( child, ypath );
		}

		virtual void addAtAddress(Field child, int align = DFLT_ALIGN);

		// must override to check that nelms == 1
		virtual void addAtAddress(Field child, unsigned nelms);

		virtual uint8_t * const getBufp() const { return buf_; }

		virtual CMemDevImpl *clone(Key &k)
		{
			return new CMemDevImpl( *this, k );
		}

		virtual ~CMemDevImpl();
};

#endif
