 //@C Copyright Notice  
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_FS_ADDR_H
#define CPSW_FS_ADDR_H

#include <cpsw_hub.h>
#include <string.h>
#include <cpsw_yaml.h>
#include <unistd.h>
#include <cpsw_mutex.h>
#include <string>

class CFSAddressImpl : public CAddressImpl {
	private:
		int         fd_;
		CMtx        mtx_;
		std::string fileName_;
		bool        seekable_;
		uint64_t    offset_;
		CTimeout    timeout_;
		bool		hasTimeout_;        

	public:
		CFSAddressImpl(AKey key, YamlState &ypath);

		CFSAddressImpl(const CFSAddressImpl &orig, AKey k)
		: CAddressImpl( orig,       k   ),
		  fd_         ( orig.fd_        ),
		  fileName_   ( orig.fileName_  ),
		  seekable_   ( orig.seekable_ ),
		  offset_     ( orig.offset_    )
		{
			if ( orig.fd_ >= 0 && ((fd_ = dup( orig.fd_ )) < 0 ) ) {
				throw ErrnoError("Unable to dup file descriptor");
			}
		}

		virtual bool
		isSeekable() const
		{ 
			return seekable_;
		}

		virtual uint64_t
		getOffset() const
		{
			return offset_;
		}

		virtual bool
		getTimeout(CTimeout *pto) const
		{
			if ( hasTimeout_ && pto ) {
				*pto = timeout_;
			}
			return hasTimeout_;
		}

		virtual void dumpYamlPart(YAML::Node &) const;

		virtual CFSAddressImpl *clone(AKey k) { return new CFSAddressImpl( *this, k ); }

		virtual int      open (CompositePathIterator *);
		virtual int      close(CompositePathIterator *);

		virtual uint64_t read (CReadArgs  *args) const;
		virtual uint64_t write(CWriteArgs *args) const;

		virtual ~CFSAddressImpl();

};

#endif
