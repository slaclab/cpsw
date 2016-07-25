#ifndef CPSW_COMMAND_H
#define CPSW_COMMAND_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_obj_cnt.h>

#include <cpsw_yaml.h>

using boost::static_pointer_cast;
using boost::weak_ptr;

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
//           The CCommand_Adapt object, OTOH is specifically
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
	CCommandImplContext(Path pParent) : pParent_(pParent) { ++sh_ocnt_(); }
	virtual ~CCommandImplContext()                        { --sh_ocnt_(); }
	const Path getParent() const { return pParent_; }    
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
};

class CCommand_Adapt;
typedef shared_ptr<CCommand_Adapt> Command_Adapt;

class CCommand_Adapt: public virtual ICommand, public virtual IEntryAdapt {
private:
        CommandImplContext pContext_;

public:
        CCommand_Adapt(Key &k, Path p, shared_ptr<const CCommandImpl> ie);

        static Command create(Path p);

        virtual void execute() { asCommandImpl()->executeCommand( pContext_ ); }

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

	virtual void dumpYamlPart(YAML::Node &) const;

	static  const char *_getClassName()       { return "SequenceCommand"; }
	virtual const char * getClassName() const { return _getClassName();   }
};

#endif
