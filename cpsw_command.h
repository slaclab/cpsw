#ifndef CPSW_COMMAND_H
#define CPSW_COMMAND_H

#include <cpsw_api_builder.h>
#include <cpsw_entry_adapt.h>

using boost::static_pointer_cast;
using boost::weak_ptr;

class CCommandImpl;
typedef shared_ptr<CCommandImpl> CommandImpl;

class CCommandImpl : public CEntryImpl, public virtual ICommandField {
public:
        virtual void executeCommand( Path pParent ) const;
        CCommandImpl(Key &k, const char* name);
};

class CCommand_Adapt;
typedef shared_ptr<CCommand_Adapt> Command_Adapt;

class CCommand_Adapt: public virtual ICommand, public virtual IEntryAdapt {
private:
        Path pParent_;

public:
        CCommand_Adapt(Key &k, Path p, shared_ptr<const CCommandImpl> ie);

        static Command create(Path p);
        virtual void execute() { asCommandImpl()->executeCommand( pParent_ ); }

protected:
        virtual shared_ptr<const CCommandImpl> asCommandImpl() const { return static_pointer_cast<const CCommandImpl, const CEntryImpl>(ie_); }
};

#endif
