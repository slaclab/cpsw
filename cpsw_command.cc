#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_command.h>
#include <cpsw_address.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

DECLARE_OBJ_COUNTER( CCommandImplContext::sh_ocnt_, "CommandImplContext", 0 )

Command ICommand::create(Path p)
{
Command_Adapt rval = IEntryAdapt::check_interface<Command_Adapt, CommandImpl>( p );
	if ( !rval ) {
		throw InterfaceNotImplementedError( p->toString() );
	}
	return rval;
}

CCommandImpl::CCommandImpl(Key &k, const char *name):
	CEntryImpl(k, name, 1)
{

}

CommandImplContext CCommandImpl::createContext(Path pParent) const
{
	return make_shared<CCommandImplContext>(pParent);
}


void CCommandImpl::executeCommand(CommandImplContext ctxt) const
{
	throw InterfaceNotImplementedError( ctxt->getParent()->toString() );
}

CCommand_Adapt::CCommand_Adapt(Key &k,  Path p, shared_ptr<const CCommandImpl> ie):
	IEntryAdapt(k, p, ie)
{
  p = p->clone();
  p->up();
  pContext_ = ie->createContext( p );
}

Command CCommand_Adapt::create(Path p)
{
	Command_Adapt comm = IEntryAdapt::check_interface<Command_Adapt, CommandImpl>( p );
	return comm;
}

