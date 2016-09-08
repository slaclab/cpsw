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

class CMemDevImpl;
typedef shared_ptr<CMemDevImpl>            MemDevImpl;
typedef shared_ptr<const CMemDevImpl> ConstMemDevImpl;

// pseudo-device with memory backing (for testing)

class CMemAddressImpl : public CAddressImpl {
	public:
		CMemAddressImpl(AKey key, unsigned nelms = 1);

		CMemAddressImpl(const CMemAddressImpl &orig, AKey k)
		: CAddressImpl(orig, k)
		{
		}

		// ANY subclass must implement clone(AKey) !
		virtual CMemAddressImpl *clone(AKey k) { return new CMemAddressImpl( *this, k ); }

		virtual uint64_t read(CompositePathIterator *node, CReadArgs *args)   const;
		virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;
};

class CMemDevImpl : public CDevImpl, public virtual IMemDev {
	private:
		uint8_t * buf_;
	protected:
		CMemDevImpl(const CMemDevImpl &orig, Key &k);

	public:
		CMemDevImpl(Key &k, const char *name, uint64_t size);

		CMemDevImpl(Key &k, YamlState &n);

		static  const char *_getClassName()       { return "MemDev";        }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual void addAtAddress(Field child, unsigned nelms = 1);

		virtual uint8_t * const getBufp() const { return buf_; }

		virtual CMemDevImpl *clone(Key &k)
		{
			return new CMemDevImpl( *this, k );
		}

		virtual ~CMemDevImpl();
};

#endif
