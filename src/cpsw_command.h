 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_COMMAND_H
#define CPSW_COMMAND_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_obj_cnt.h>

#include <cpsw_yaml.h>

using cpsw::static_pointer_cast;
using cpsw::weak_ptr;

class CCommandImpl;
typedef shared_ptr<CCommandImpl> CommandImpl;

class CCommandImplContext;
typedef shared_ptr<CCommandImplContext> CommandImplContext;

// Context info for 'CommandImpl'
// Reminder: 'CommandImpl' implements the command algorithm
//           but cannot hold any mutable state information
//           because the same 'CommandImpl' object could
//           be attached at multiple places in the hierarchy.
//           This is reflected by the fact that the member
//           functions of CCommandImpl are 'const' (unable
//           to alter the state of CCommandImpl).
//
//           The CCommandAdapt object, OTOH is specifically
//           created for a given 'Path' and may thus hold
//           related context. The Adapter does not have to
//           know about the semantics nor contents of the
//           context. It lets the CCommandImpl (or derived
//           class) create a suitable context (from the Path
//           associated with the adapter) and passes that
//           context when executing the command.
//
//           As an example:
//
//           A certain command (derived from CCommandImpl might
//           need a ScalVal from its 'executeCommand' method.
//
//           It could either create (and destroy) such
//           a ScalVal "on the fly" each time executeCommand()
//           runs (the ScalVal would then be a local variable).
//           It would use the 'pParent_' member from the context.
//
//           Alternatively, a new context class could be
//           derived from CCommandImplContext with a ScalVal member.
//           This member would be created once (from createContext())
//           and reused every time the command executes.
//           See cpsw_command_tst.cc for an example.
//
//           WARNING: when keeping shared pointers in the context
//           be careful not to create circular references (e.g.,
//           command A has a pointer to command B in its context
//           and command B one to command A) !

class CCommandImplContext {
private:
	static CpswObjCounter & sh_ocnt_();
	CCommandImplContext( const CCommandImplContext & );
protected:
	Path   pParent_;
public:
	CCommandImplContext(Path pParent) : pParent_(pParent)
	{
		++sh_ocnt_();
	}

	virtual ~CCommandImplContext()
	{
		--sh_ocnt_();
	}

	virtual ConstPath getParent() const
	{
		return pParent_;
	}
};

class CCommandImpl : public CEntryImpl {
public:
		// create a context from the pParent Path
		virtual CommandImplContext createContext( Path pParent )   const;

		// context is passed back to 'executeCommand'
		virtual void executeCommand( CommandImplContext pContext ) const;

		CCommandImpl(Key &k, const char* name);

		CCommandImpl(Key  &k, YamlState &node)
		: CEntryImpl(k, node)
		{
		}

		CCommandImpl(const CCommandImpl &orig, Key &k)
		: CEntryImpl(orig, k)
		{
		}

		virtual IVal_Base::Encoding getEncoding() const
		{
			return IVal_Base::NONE;
		}

		virtual CCommandImpl *clone(Key &k)
		{
			return new CCommandImpl( *this, k );
		}

		virtual uint64_t dumpMyConfigToYaml(Path, YAML::Node &) const;
		virtual uint64_t loadMyConfigFromYaml(Path, YAML::Node &) const;

		virtual EntryAdapt createAdapter(IEntryAdapterKey &key, Path p, const std::type_info &interfaceType) const;
};

class CCommandAdapt;
typedef shared_ptr<CCommandAdapt> CommandAdapt;

class CCommandAdapt: public virtual ICommand, public virtual IEntryAdapt {
private:
		CommandImplContext pContext_;

public:
		CCommandAdapt(Key &k, Path p, shared_ptr<const CCommandImpl> ie);

		virtual Encoding  getEncoding()  const;
		virtual Path      getPath()      const;
		virtual ConstPath getConstPath() const;

		static Command create(Path p);

		virtual void execute();

protected:
		virtual shared_ptr<const CCommandImpl> asCommandImpl() const { return static_pointer_cast<const CCommandImpl, const CEntryImpl>(ie_); }
};

class CSequenceCommandImpl;
typedef shared_ptr<CSequenceCommandImpl> SequenceCommandImpl;

class CSequenceCommandImpl : public CCommandImpl, public virtual ISequenceCommand {
private:
	Items items_;
public:
	CSequenceCommandImpl(Key &k, const char *name, const Items *items_p);
	virtual void executeCommand(CommandImplContext context) const;
	CSequenceCommandImpl(Key &k, YamlState &node);

	CSequenceCommandImpl(const CSequenceCommandImpl &orig, Key &k)
	: CCommandImpl( orig, k ),
	  items_( orig.items_ )
	{
	}

	virtual CSequenceCommandImpl *clone(Key &k)
	{
		return new CSequenceCommandImpl( *this, k );
	}

	virtual void dumpYamlPart(YAML::Node &) const;

	static  const char *_getClassName()       { return "SequenceCommand"; }
	virtual const char * getClassName() const { return _getClassName();   }
};

#endif
