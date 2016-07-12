#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_command.h>
#include <cpsw_address.h>

#include <cpsw_error.h>

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

SequenceCommand ISequenceCommand::create(const char* name, const Items *items_p)
{
	return CShObj::create<SequenceCommandImpl>( name, items_p );
}

CSequenceCommandImpl::CSequenceCommandImpl(Key &k, const char *name, const Items *items_p)
: CCommandImpl(k, name),
  items_( *items_p )
{

}

#ifdef WITH_YAML
const char * const CSequenceCommandImpl::className_ = "SequenceCommand";

CSequenceCommandImpl::CSequenceCommandImpl(Key &key, const YAML::Node &node)
: CCommandImpl(key, node)
{
const YAML::Node &seq_node( node["sequence"] );
	if ( ! node ) {
		throw InvalidArgError(std::string(getName()) + " no sequence found");
	} 
	if ( ! seq_node.IsSequence() ) {
		throw InvalidArgError(std::string(getName()) + " YAML node not a sequence");
	}

	for ( YAML::const_iterator it( seq_node.begin() ); it != seq_node.end(); ++it ) {
		std::string entry;
		uint64_t    value;

		mustReadNode( *it, "entry", &entry );
		mustReadNode( *it, "value", &value );
		items_.push_back( Item( entry, value ) );
	}
}

void
CSequenceCommandImpl::dumpYamlPart(YAML::Node &node) const
{
	CCommandImpl::dumpYamlPart( node );

	Items::const_iterator it( items_.begin() );

	while ( it != items_.end() ) {
		YAML::Node item_node;
		item_node["entry"] = (*it).first;
		item_node["value"] = (*it).second;
		node["sequence"].push_back( item_node );
		++it;
	}
}

#endif

void CSequenceCommandImpl::executeCommand(CommandImplContext context) const
{
	const Path parent( context->getParent() );

	Path p;

	for( Items::const_iterator it = items_.begin(); it != items_.end(); ++it )
	{
		if( (*it).first.compare(0, 6, "usleep") == 0 ) {
			usleep( (useconds_t) (*it).second );
		}
		else {
			try {
				Path p( parent->findByName( (*it).first.c_str() ) );

				try {
					// if ScalVal
#if 0
					// without caching and bit-level access at the SRP protocol level we cannot
					// support write-only yet.
					ScalVal_WO s( IScalVal_WO::create( p ) );
#else
					ScalVal s( IScalVal::create( p ) );
#endif
					//set all nelms to same value if addressing multiple
					s->setVal( (*it).second );

				} catch (InterfaceNotImplementedError &e) {
					// else if command
					Command c( ICommand::create( p ) );
					c->execute();
				}
			} catch (CPSWError &e) {
				std::string str = "(SequenceCommand: " + p->toString() + ") ";
				e.prepend( str );
				throw;
			}
		}
	}
}


