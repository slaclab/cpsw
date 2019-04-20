 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_NULL_TST_DEV_H
#define CPSW_NULL_TST_DEV_H

#include <cpsw_hub.h>
#include <cpsw_yaml.h>

class CNullDevImpl;
typedef shared_ptr<CNullDevImpl>            NullDevImpl;
typedef shared_ptr<const CNullDevImpl> ConstNullDevImpl;

// pseudo-device with memory backing (for testing)

class CNullAddressImpl : public CAddressImpl {
	public:
		CNullAddressImpl(AKey key);

		CNullAddressImpl(const CNullAddressImpl &orig, AKey k)
		: CAddressImpl(orig, k)
		{
		}

		CNullAddressImpl(AKey k, YamlState &ypath);

		// ANY subclass must implement clone(AKey) !
		virtual CNullAddressImpl *clone(AKey k) { return new CNullAddressImpl( *this, k ); }

		virtual uint64_t read(CompositePathIterator *node, CReadArgs *args)   const;
		virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;
};

class CNullDevImpl : public CDevImpl, public virtual INullDev {
	protected:
		CNullDevImpl(const CNullDevImpl &orig, Key &k);

	public:
		CNullDevImpl(Key &k, const char *name, uint64_t size);

		CNullDevImpl(Key &k, YamlState &n);

		static  const char *_getClassName()       { return "NullDev";        }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual void addAtAddress(Field child);

		// must override to check that nelms == 1
		virtual void addAtAddress(Field child, unsigned nelms);

		virtual void addAtAddress(Field child, YamlState &ypath)
		{
			doAddAtAddress<CNullAddressImpl>( child, ypath );
		}

		virtual CNullDevImpl *clone(Key &k)
		{
			return new CNullDevImpl( *this, k );
		}

		virtual ~CNullDevImpl();
};

#endif
