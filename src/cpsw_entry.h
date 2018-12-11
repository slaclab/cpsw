 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_ENTRY_H
#define CPSW_ENTRY_H

#include <cpsw_api_builder.h>
#include <cpsw_shared_obj.h>
#include <cpsw_mutex.h>


#include <cpsw_yaml.h>

#include <typeinfo>

using cpsw::weak_ptr;

class   Visitor;

class   CEntryImpl;
typedef shared_ptr<CEntryImpl>        EntryImpl;

class   CDevImpl;
typedef shared_ptr<const CDevImpl>    ConstDevImpl;

class IEntryAdapterKey;

class IEntryAdapt;
typedef shared_ptr<IEntryAdapt>       EntryAdapt;

class CEntryImpl: public virtual IField, public CShObj, public CYamlSupportBase {
	public:
		static const int CONFIG_PRIO_OFF      = 0;
		static const int DFLT_CONFIG_PRIO     = CONFIG_PRIO_OFF;
		static const int DFLT_CONFIG_PRIO_DEV = 1; // on for containers
		static double    DFLT_POLL_SECS()     { return -1.0; }

		class                              CUniqueHandle;
		typedef shared_ptr<CUniqueHandle>   UniqueHandle;

	private:
		// WARNING -- when modifying fields you might need to
		//            modify 'operator=' as well as the copy
		//            constructor!
		std::string	        name_;
		std::string         description_;
	protected:
		uint64_t    	    size_;
	private:

		class CUniqueListHead {
		protected:
			CUniqueHandle *n_;

		public:
			CUniqueListHead()
			: n_(0)
			{
			}

			CUniqueHandle *getNext()
			{
				return n_;
			}

			void setNext(CUniqueHandle *n)
			{
				n_ = n;
			}

			~CUniqueListHead() throw();
		};

		mutable Cacheable       cacheable_;
		mutable int             configPrio_;
		mutable bool            configPrioSet_;
		mutable double          pollSecs_;
		mutable bool            locked_;
		mutable CMtxLazy        uniqueListMtx_;
		mutable CUniqueListHead uniqueListHead_;

		CEntryImpl(const CEntryImpl &);
		CEntryImpl &operator=(const CEntryImpl&);


	private:

		void checkArgs();

	protected:
		// prevent public copy construction -- cannot copy the 'self'
		// member. This constructor is intended to be used by the 'clone'
		// member which takes care of setting 'self'.
		CEntryImpl(const CEntryImpl &ei, Key &k);

		// subclasses may want to define their default configuration priority.
		// This just helps to reduce the size of the YAML file since default
		// values are not stored.
		virtual int getDefaultConfigPrio() const;

		virtual double getDefaultPollSecs() const;

	public:
		CEntryImpl(Key &k, const char *name, uint64_t size);

		CEntryImpl(Key &k, YamlState &n);

		virtual void dumpYamlPart(YAML::Node &) const;

		virtual uint64_t dumpMyConfigToYaml(Path, YAML::Node &) const;
		virtual uint64_t loadMyConfigFromYaml(Path, YAML::Node &) const;

		virtual uint64_t processYamlConfig(Path, YAML::Node &, bool) const;

		// Every subclass MUST implement the 'getClassName()' virtual
		// method. Just copy-paste this one:
		static  const char *_getClassName()       { return "Field";         }
		virtual const char * getClassName() const { return _getClassName(); }

		virtual CEntryImpl *clone(Key &k) { return new CEntryImpl( *this, k ); }

		virtual const char *getName() const
		{
			return name_.c_str();
		}

		virtual const char *getDescription() const
		{
			return description_.c_str();
		}

		virtual double getPollSecs() const
		{
			return pollSecs_;
		}

		virtual void setDescription(const char *);
		virtual void setDescription(const std::string&);

		virtual uint64_t getSize() const
		{
			return size_;
		}

		virtual Cacheable getCacheable() const
		{
			return cacheable_;
		}

		virtual int getConfigPrio() const
		{
			return configPrio_;
		}

		// may throw exception if modified after
		// being attached
		virtual void setCacheable(Cacheable cacheable);

		// may throw exception if modified after
		// being attached
		virtual void setConfigPrio(int configPrio);

		// may throw exception if modified after
		// being attached
		virtual void setPollSecs(double pollSecs);

		virtual ~CEntryImpl();

		virtual void setLocked()
		{
			locked_ = true;
		}

		virtual bool getLocked()
		{
			return locked_;
		}

		virtual void postHook( ConstShObj );

		virtual void accept(IVisitor    *v, RecursionOrder order, int recursionDepth);

		virtual EntryImpl getSelf()            { return getSelfAs<EntryImpl>();      }
		virtual EntryImpl getConstSelf() const { return getSelfAsConst<EntryImpl>(); }

		virtual Hub isHub()                   const { return Hub();          }
		virtual ConstDevImpl isConstDevImpl() const { return ConstDevImpl(); }

		// obtain a unique handle. No other EntryAdapt can be created
		// for a path that intersects with 'adapt's path until
		// the handle goes out of scope.

		// This member is not for general use (therefore the key argument)
		virtual UniqueHandle getUniqueHandle(IEntryAdapterKey &key, ConstPath p) const;

		// This member is not for general use (therefore the key argument)
		virtual EntryAdapt createAdapter(IEntryAdapterKey &key, Path p, const std::type_info &interfaceType) const;

		virtual void dump(FILE *) const;
		virtual void dump()       const { dump( ::stdout ); }

protected:
		// You can use this template to implement the virtual 'createAdapter' member
		// The 'helper' argument exists for convenience (template argument deduction)
		template <typename ADAPT, typename IMPL> EntryAdapt _createAdapter(const IMPL *helper, Path p) const
		{
			return CShObj::template create<ADAPT>(p, getSelfAsConst<shared_ptr<const IMPL> >());
		}

		template <typename INTRF> static bool isInterface(const std::type_info &interfaceType)
		{
			return typeid(typename INTRF::element_type) == interfaceType;
		}
};

#endif
