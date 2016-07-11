#include <cpsw_api_builder.h>
#include <cpsw_command.h>
#include <udpsrv_regdefs.h>

#include <stdio.h>

class CMyCommandImpl;
typedef shared_ptr<CMyCommandImpl> MyCommandImpl;

// Context for CMyCommandImpl; we want to keep
// a ScalVal around for re-use every time the
// command executes.
class CMyContext : public CCommandImplContext {
private:
	ScalVal theVal_;
public:
	CMyContext(Path p)
	: CCommandImplContext( p ),
	  theVal_( IScalVal::create( p->findByName( "val" ) ) )
	{
	}

	ScalVal getVal()
	{
		return theVal_;
	}
};

class CMyCommandImpl : public CCommandImpl {
public:

	// make a new CMyContext
	virtual CommandImplContext createContext(Path pParent) const
	{
		return make_shared<CMyContext>(pParent);
	}

	// execute the command using the ScalVal stored
	// in CMyContext
	virtual void executeCommand(CommandImplContext context) const
	{
		shared_ptr<CMyContext> myContext( static_pointer_cast<CMyContext>(context) );
		myContext->getVal()->setVal( 0xdeadbeef );
	}

	CMyCommandImpl(Key &k, const char *name)
	: CCommandImpl(k, name)
	{
	}

	static Field create(const char *name)
	{
		return CShObj::create<MyCommandImpl>( name );
	}
};

class TestFailed {};

int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
try {

NetIODev root( INetIODev::create("root", ip_addr ) );
MMIODev  mmio( IMMIODev::create( "mmio", MEM_SIZE) );

	mmio->addAtAddress( IIntField::create("val" , 32     ), REGBASE+REG_SCR_OFF );
	mmio->addAtAddress( CMyCommandImpl::create("cmd"), 0 );

        ISequenceCommand::commandSequence commandSeq;
        ISequenceCommand::Cmd c1("usleep", 1000);
        ISequenceCommand::Cmd c2("val", 0x1);
	commandSeq.push_back( c1 );
	commandSeq.push_back( c2 );
	mmio->addAtAddress( ISequenceCommand::create("seqCmd", commandSeq), 0 );

	root->addAtAddress( mmio, INetIODev::createPortBuilder() );

	ScalVal val( IScalVal::create( root->findByName("mmio/val") ) );

	Command cmd( ICommand::create( root->findByName("mmio/cmd") ) );
	Command seqCmd( ICommand::create( root->findByName("mmio/seqCmd") ) );
	uint64_t v = -1ULL;

	val->setVal( (uint64_t)0 );
	val->getVal( &v, 1 );
	if ( v != 0 ) {
		throw TestFailed();
	}

	cmd->execute();

	val->getVal( &v, 1 );
	if ( v != 0xdeadbeef ) {
		throw TestFailed();
	}

	seqCmd->execute();

	val->getVal( &v, 1 );
	if ( v != 0x1 ) {
		throw TestFailed();
	}


} catch (CPSWError e) {
	fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
	throw e;
}

	fprintf(stderr,"Command test PASSED\n");
	return 0;
}
