#ifndef CPSW_COMMAND_H
#define CPSW_COMMAND_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_obj_cnt.h>

using boost::static_pointer_cast;
using boost::weak_ptr;

class CCommandImpl;
typedef shared_ptr<CCommandImpl> CommandImpl;

class CCommandImplContext;
typedef shared_ptr<CCommandImplContext> CommandImplContext;


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

class CCommandImpl : public CEntryImpl, public virtual ICommandField {
public:
		// derived classes should chain through this method
		virtual CommandImplContext createContext( Path pParent )   const;
        virtual void executeCommand( CommandImplContext pContext ) const;
        CCommandImpl(Key &k, const char* name);
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

#endif
