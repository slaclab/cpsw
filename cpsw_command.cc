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

SequenceCommand ISequenceCommand::create(const char* name, std::vector<std::string> entryPath, std::vector<uint64_t> values)
{
	return CShObj::create<SequenceCommandImpl>( name, entryPath, values );
}

CSequenceCommandImpl::CSequenceCommandImpl(Key &k, const char *name, std::vector<std::string> names, std::vector<uint64_t> values):
	CCommandImpl(k, name),
	names_(names),
	values_(values)
{

}

CommandImplContext CSequenceCommandImpl::createContext(Path pParent) const
{
	return make_shared<CSequenceCommandContext>(pParent, names_, values_);
}

void CSequenceCommandImpl::executeCommand(CommandImplContext context) const
{
	shared_ptr<CSequenceCommandContext> myContext( static_pointer_cast<CSequenceCommandContext>(context) );
	myContext->executeSequence(names_, values_);
}


CSequenceCommandContext::CSequenceCommandContext(Path p, std::vector<std::string> names, std::vector<uint64_t> values):
	CCommandImplContext( p )
{
	ScalVal s;
	/* Check that we have a valid command set or throw an error */
	try {
		for( std::vector<std::string>::iterator it = names.begin(); it != names.end(); ++it )
		{
			if( (*it).compare(0, 6, "usleep") == 0 ) {
			}
			else {
				p->findByName( (*it).c_str() );
			}
		}
	} catch( CPSWError &e ) {
		std::string str = "SequenceCommand invalid argument: " + e.getInfo();
		throw InvalidArgError(str.c_str());
	}
}

void CSequenceCommandContext::executeSequence(std::vector<std::string> names, std::vector<uint64_t> values)
{
	Path p;
	Command c;
#if 0
// without caching and bit-level access at the SRP protocol level we cannot
// support write-only yet.
	ScalVal_WO s;
#else
	ScalVal s;
#endif
	int j = 0;
	uint64_t v;
	try {
		for( std::vector<std::string>::iterator it = names.begin(); it != names.end(); ++it, j++ )
		{
			if( (*it).compare(0, 6, "usleep") == 0 ) {
				usleep( (useconds_t) values[j] );
			}
			else {
				try {
					p = pParent_->findByName( (*it).c_str() );
				}
				catch( CPSWError &e ) {
					std::string str = "SequenceCommand invalid path: " + *(it);
					throw InvalidArgError(str.c_str());
				}
				try {
					// if ScalVal
#if 0
// without caching and bit-level access at the SRP protocol level we cannot
// support write-only yet.
					s = IScalVal_WO::create( p );
#else
					s = IScalVal::create( p );
#endif
					//set all nelms to same value if addressing multiple
					s->setVal( values[j] );
					continue;
				} 
				catch( CPSWError &e ) {
				}
				try {
					// else if command
					c = ICommand::create( p );
					c->execute();
					continue;
				}
				catch( CPSWError &e ) {
					std::string str = "SequenceCommand invalid arg: " + p->toString();
					throw InvalidArgError(str.c_str());
				}
			}
		}
	} catch( CPSWError &e ) {
		throw e;
	}

	return;
}


