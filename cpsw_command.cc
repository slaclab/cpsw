#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_command.h>
#include <cpsw_address.h>

Command ICommand::create(Path p)
{
Command_Adapt rval = IEntryAdapt::check_interface<Command_Adapt, CommandImpl>( p );
        if ( !rval ) {
                        throw InterfaceNotImplementedError( p );
        }
        return rval;
}

CommandField ICommandField::create(const char *name)
{
        return CShObj::create<CommandImpl>(name, 1);
}

CCommandImpl::CCommandImpl( Key &k, const char *name, uint64_t size):
	CEntryImpl(k, name, size)
{

}

CCommand_Adapt::CCommand_Adapt(Key &k,  Path p, shared_ptr<const CCommandImpl> ie):
	IEntryAdapt(k, p, ie)
{
	pParent_= p->clone(); 
	pParent_->up(); 
}

Command CCommand_Adapt::create(Path p)
{
	Command_Adapt comm = IEntryAdapt::check_interface<Command_Adapt, CommandImpl>( p );
	return comm;

}

