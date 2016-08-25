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

YAML::Node
CCommandImpl::dumpMyConfigToYaml(Path p) const
{
// return a node; there is really no
// configuration data to be saved but we do need
// a placeholder.
return YAML::Node("exec"); //just a non-Null node
}

void
CCommandImpl::loadMyConfigFromYaml(Path p, YAML::Node &node) const
{
	ICommand::create( p )->execute();
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

void
CCommand_Adapt::execute()
{
	asCommandImpl()->executeCommand( pContext_ );
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

CSequenceCommandImpl::CSequenceCommandImpl(Key &key, YamlState &node)
: CCommandImpl(key, node)
{
YamlState &seq_node( node.lookup("sequence") );
	if ( ! seq_node ) {
		throw InvalidArgError(std::string(getName()) + " no sequence found");
	} 
	if ( ! seq_node.IsSequence() ) {
		throw InvalidArgError(std::string(getName()) + " YAML node not a sequence");
	}

	for ( unsigned i = 0; i < seq_node.size(); ++i ) {
		YAML::PNode item_node( &seq_node, i );
		std::string entry;
		uint64_t    value;

		mustReadNode( item_node, "entry", &entry );
		mustReadNode( item_node, "value", &value );
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
		writeNode(item_node, "entry", (*it).first );
		writeNode(item_node, "value", (*it).second);
		pushNode(node, "sequence", item_node);
		++it;
	}
}

void CSequenceCommandImpl::executeCommand(CommandImplContext context) const
{
	ConstPath parent( context->getParent() );

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
				std::string str = "(SequenceCommand: " + parent->toString() + "/" + (*it).first + ") ";
				e.prepend( str );
				throw;
			}
		}
	}
}


