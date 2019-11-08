 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_api_builder.h>
#include <cpsw_command.h>

#include <stdio.h>

#include <cpsw_myCommand.h>

class TestFailed {};

// Demo for how to create a new CPSW class (a 'Command' in this case).

// It can be dynamically added to a CPSW application

class CMyCommandImpl;
typedef shared_ptr<CMyCommandImpl> MyCommandImpl;

// Context for CMyCommandImpl; demonstrate how to e.g., keep
// a ScalVal around for re-use every time the command executes.

// If you want to do this in a real-world class probably depends.
// Only worthwhile if a ScalVal is reused very frequently.

// At any rate - this shows how any type of context info could be
// dealt with.
class CMyContext : public CCommandImplContext {
private:
	ScalVal theVal_;
public:
	CMyContext(Path p, const char *rel_path)
	: CCommandImplContext( p ),
	  theVal_( IScalVal::create( p->findByName( rel_path ) ) )
	{
	}

	ScalVal getVal()
	{
		return theVal_;
	}
};

class CMyCommandImpl : public CCommandImpl, public IMyCommandImpl {
private:
    CString connectTo_;
public:

	// make a new CMyContext
	virtual CommandImplContext createContext(Path pParent) const
	{
		return cpsw::make_shared<CMyContext>(pParent, connectTo_->c_str());
	}

	// execute the command using the ScalVal stored
	// in CMyContext
	virtual void executeCommand(CommandImplContext context) const
	{
		uint64_t v;
		shared_ptr<CMyContext> myContext( cpsw::static_pointer_cast<CMyContext>(context) );

		myContext->getVal()->getVal( &v );
		if ( v != 0 )
			throw TestFailed();
		myContext->getVal()->setVal( 0xdeadbeef );
	}

	// constructor
	CMyCommandImpl(Key &k, const char *name, const char *connectTo)
	: CCommandImpl(k, name),
	  connectTo_( cpsw::make_shared<const std::string>(connectTo) )
	{
	}

	// construct from a YAML node; extract 'our' information
	CMyCommandImpl(Key &k, YamlState *node)
	: CCommandImpl(k, node)
	{
	std::string connectTo;
		mustReadNode( node, "connectTo", &connectTo );
		connectTo_ = cpsw::make_shared< const std::string >(connectTo);
	}

	// copy constructor (for cloning)
	CMyCommandImpl(const CMyCommandImpl &orig, Key &k)
	: CCommandImpl(orig, k),
	  connectTo_(orig.connectTo_)
	{
	}

	virtual CMyCommandImpl *clone(Key &k)
	{
		return new CMyCommandImpl( *this, k );
	}


	// save 'our' information to a yaml node
    virtual void dumpYamlPart(YAML::Node &node) const
	{
		// normally only save if current value is different from
		// the default -- but there is no default for this one.
		node["connectTo"] = connectTo_->c_str();
	}

	// Every class MUST implement these and use a unique name!
	static  const char *_getClassName()       { return "MyCommand";     }
	virtual const char *getClassName()  const { return _getClassName(); }
};

// Builder API support

Field
IMyCommandImpl::create(const char *name, const char *connectTo)
{
	return CShObj::create<MyCommandImpl>( name, connectTo );
}

// An 'extern "C"' binding is only here for sake of the test 
// program. It would not be necessary in a normal environment!

extern "C" Field IMyCommandImpl_create(const char *name, const char *connectTo)
{
	return IMyCommandImpl::create(name, connectTo);
}

// This macro will result in our class being registered with CPSW
// as soon as the shared object is loaded.

DECLARE_YAML_FIELD_FACTORY(MyCommandImpl);
