 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_user.h>
#include <cpsw_path.h>
#include <cpsw_entry_adapt.h>
#include <cpsw_command.h>
#include <cpsw_address.h>

#include <cpsw_error.h>
#include <string>

#include <iostream>


DECLARE_OBJ_COUNTER( CCommandImplContext::sh_ocnt_, "CommandImplContext", 0 )

Command ICommand::create(ConstPath p)
{
	return IEntryAdapt::check_interface<Command>( p );
}

CCommandImpl::CCommandImpl(Key &k, const char *name):
	CEntryImpl(k, name, 1)
{

}

uint64_t
CCommandImpl::dumpMyConfigToYaml(Path p, YAML::Node &n) const
{
// return a node; there is really no
// configuration data to be saved but we do need
// a placeholder.
	n = YAML::Node("exec"); //just a non-Null node
	return 1;
}

uint64_t
CCommandImpl::loadMyConfigFromYaml(Path p, YAML::Node &node) const
{
	ICommand::create( p )->execute();
	return 1;
}

CommandImplContext CCommandImpl::createContext(Path pParent) const
{
	return cpsw::make_shared<CCommandImplContext>(pParent);
}


void CCommandImpl::executeCommand(CommandImplContext ctxt) const
{
	throw InterfaceNotImplementedError( ctxt->getParent()->toString() );
}

EntryAdapt
CCommandImpl::createAdapter(IEntryAdapterKey &key, ConstPath p, const std::type_info &interfaceType) const
{
	if ( isInterface<Command>( interfaceType ) ) {
        return _createAdapter<CommandAdapt>(this, p);

	}
	return CEntryImpl::createAdapter(key, p, interfaceType);
}

CCommandAdapt::CCommandAdapt(Key &k,  ConstPath pc, shared_ptr<const CCommandImpl> ie):
	IEntryAdapt(k, pc, ie)
{
Path p = pc->clone();

	p->up();
	pContext_ = ie->createContext( p );

    open();
}

CCommandAdapt::~CCommandAdapt()
{
	close();
}

IVal_Base::Encoding
CCommandAdapt::getEncoding() const
{
	return asCommandImpl()->getEncoding();	
}

Path
CCommandAdapt::getPath() const
{
	return IEntryAdapt::getPath();
}

ConstPath
CCommandAdapt::getConstPath() const
{
	return IEntryAdapt::getConstPath();
}

void
CCommandAdapt::execute()
{
	asCommandImpl()->executeCommand( pContext_ );
}

void
CCommandAdapt::execute(int64_t arg)
{
	asCommandImpl()->executeCommand( pContext_, arg );
}

void
CCommandAdapt::execute(const char *arg)
{
	asCommandImpl()->executeCommand( pContext_, arg );
}

Enum
CCommandAdapt::getEnum() const
{
	return asCommandImpl()->getEnum();
}

SequenceCommand ISequenceCommand::create(const char* name, const Items *items_p)
{
	return CShObj::create<SequenceCommandImpl>( name, items_p );
}

CSequenceCommandImpl::CSequenceCommandImpl(Key &k, const char *name, const Items *items_p)
: CCommandImpl(k, name),
  items_( *items_p )
{
	index_.push_back( 0 );
	// always keep a marker at the end...
	index_.push_back( items_.size() );
}

void
CSequenceCommandImpl::parseItems( YamlState *seq_node )
{
unsigned i;

	for ( i = 0; i < seq_node->size(); ++i ) {
		YAML::PNode item_node( seq_node, i );
		std::string entry;
		uint64_t    value;

		mustReadNode( item_node, YAML_KEY_entry, &entry );
		mustReadNode( item_node, YAML_KEY_value, &value );
		items_.push_back( Item( entry, value ) );
	}
	// always keep a marker at the end...
	index_.push_back( index_.back() + i );

}

CSequenceCommandImpl::CSequenceCommandImpl(Key &key, YamlState &node)
: CCommandImpl(key, node)
{
YamlState &seq_node( node.lookup( YAML_KEY_sequence ) );

	if ( ! seq_node ) {
		throw ConfigurationError( std::string( getName() ) + ": no sequence found" );
	} 
	if ( ! seq_node.IsSequence() ) {
		throw ConfigurationError( std::string( getName() ) + ": YAML node not a sequence" );
	}

	YamlState &enum_node( node.lookup( YAML_KEY_enums ) );

	if ( enum_node ) {
		enums_ = IMutableEnum::create( enum_node );
	}

	index_.push_back( 0 );

	if ( 0 == seq_node.size() ) {
		throw ConfigurationError( std::string(getName()) + ": sequence is empty" );
	}

	for ( unsigned i = 0; i < seq_node.size(); ++i ) {

		YAML::PNode item_node( &seq_node, i );

		if ( ! item_node.IsSequence() ) {
			if ( 0 == i ) {
				// single sequence case; this also terminates the 'index_' if the sequence is empty
				parseItems( &seq_node );
				break;
			}
			throw ConfigurationError( std::string(getName()) + ": YAML node not a sequence of sequences" );
		}
		// A sequence of sequences, i.e., multiple commands
		parseItems( &item_node );
	}

	if ( enums_ && enums_->getNelms() != index_.size() - 1 ) {
		throw ConfigurationError( std::string(getName()) + ": enum must have as many elements as the sequences" );
	}
}

void
CSequenceCommandImpl::dumpYamlPart(YAML::Node &node) const
{
	CCommandImpl::dumpYamlPart( node );

	Items::const_iterator it( items_.begin() );

	YAML::Node seq_node( YAML::NodeType::Sequence );

	if ( index_.size() > 2 ) {
		for ( unsigned i = 0; i < index_.size() - 1; ++i ) {
			YAML::Node item_seq_node( YAML::NodeType::Sequence );
			for ( int j = index_[i]; j < index_[i+1]; j++ ) {
				YAML::Node item_node( YAML::NodeType::Map );
				writeNode(item_node, YAML_KEY_entry, (*it).first );
				writeNode(item_node, YAML_KEY_value, (*it).second);
				item_seq_node.push_back( item_node );
				++it;
			}
			seq_node.push_back( item_seq_node );
		}
	} else {
		while ( it != items_.end() ) {
			YAML::Node item_node( YAML::NodeType::Map );
			writeNode(item_node, YAML_KEY_entry, (*it).first );
			writeNode(item_node, YAML_KEY_value, (*it).second);
			seq_node.push_back( item_node );
			++it;
		}
	}

	writeNode(node, YAML_KEY_sequence, seq_node);

	if ( enums_ ) {
		YAML::Node enums;
		
		enums_->dumpYaml( enums );
		writeNode(node, YAML_KEY_enums, enums); 
	}
}

void CSequenceCommandImpl::executeCommand(CommandImplContext context, int64_t arg) const
{
ConstPath parent( context->getParent() );

	if ( arg < 0 || arg >= (int64_t)index_.size() - 1 ) {
		throw InvalidArgError( std::string("CSequenceCommandImpl: ") + parent->toString() + "/" + getName() + " - integer arg out of range" );
	}

	Path p;

	Items::const_iterator it  = items_.begin() + index_[arg    ];
	Items::const_iterator ite = items_.begin() + index_[arg + 1];


	for( ; it != ite; ++it )
	{
		if( (*it).first.compare(0, 6, "usleep") == 0 ) {
			usleep( (useconds_t) (*it).second );
		}
		else if ( (*it).first.compare(0, 7, "system(") == 0 && ( ')' == (*it).first.at( (*it).first.length() - 1 ) ) ) {
			int         status;
			std::string cmd( (*it).first, 7, (*it).first.length() - 8 );

			if ( ( status = system( cmd.c_str() ) ) ) {
				char sbuf[100];
				snprintf(sbuf, sizeof(sbuf), "%d", status);
				throw InvalidArgError(
				    std::string( "CSequenceCommandImpl: 'system' execution of '" )
				    + cmd
				    + std::string( "' failed -- status = ")
					+ std::string( sbuf                   )
				);
			}
		}
		else {
			try {
				Path p( parent->findByName( (*it).first.c_str() ) );

				try {
					// if ScalVal
					ScalVal_WO s( IScalVal_WO::create( p ) );
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

void CSequenceCommandImpl::executeCommand(CommandImplContext context) const
{
	if ( index_.size() > 2 ) {
		ConstPath parent( context->getParent() );
		throw InvalidArgError( std::string("CSequenceCommandImpl: ") + parent->toString() + "/" + getName() + " - need integer argument" );
	}
	executeCommand( context, (int64_t)0 );
}

void CSequenceCommandImpl::executeCommand(CommandImplContext context, const char *arg) const
{
	if ( ! enums_ ) {
		ConstPath parent( context->getParent() );
		throw InvalidArgError( std::string("CSequenceCommandImpl: ") + parent->toString() + "/" + getName() + " - command has no enums; string arg not supported" );
	}

	IEnum::Item i = enums_->map( arg );
	
	executeCommand( context, i.second );
}

