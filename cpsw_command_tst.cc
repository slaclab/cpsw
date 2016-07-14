#include <cpsw_api_builder.h>
#include <cpsw_command.h>
#include <udpsrv_regdefs.h>
#include <vector>
#include <string>

#include <stdio.h>

#ifdef WITH_YAML
#include <cpsw_yaml.h>
#endif

class TestFailed {};

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
		uint64_t v;
		shared_ptr<CMyContext> myContext( static_pointer_cast<CMyContext>(context) );

		myContext->getVal()->getVal( &v );
		if ( v != 0 )
			throw TestFailed();
		myContext->getVal()->setVal( 0xdeadbeef );
	}

	CMyCommandImpl(Key &k, const char *name)
	: CCommandImpl(k, name)
	{
	}

#ifdef WITH_YAML
	CMyCommandImpl(Key &k, const YAML::Node &node)
	: CCommandImpl(k, node)
	{
	}

	static  const char *_getClassName()       { return "MyCommand";     }
	virtual const char *getClassName()  const { return _getClassName(); }
#endif

	static Field create(const char *name)
	{
		return CShObj::create<MyCommandImpl>( name );
	}
};

#ifdef WITH_YAML
DECLARE_YAML_FIELD_FACTORY(MyCommandImpl);
#endif

int
main(int argc, char **argv)
{
const char *ip_addr = "127.0.0.1";
int         opt;
const char *use_yaml = 0;
const char *dmp_yaml = 0;

	while ( (opt = getopt(argc, argv, "y:Y:")) > 0 ) {
		switch (opt) {
			default:
				fprintf(stderr,"Unknown option -'%c'\n", opt);
				throw TestFailed();
#ifdef WITH_YAML
			case 'Y': use_yaml    = optarg;   break;
			case 'y': dmp_yaml    = optarg;   break;
#else
			case 'y':
			case 'Y': fprintf(stderr,"YAML support not compiled in\n");
				throw TestFailed();
#endif
		}
	}

try {

Hub root;

	if ( use_yaml ) {
#ifdef WITH_YAML
		root = IHub::loadYamlFile( use_yaml, "root" );
#endif
	} else {

	NetIODev netio( INetIODev::create("root", ip_addr ) );
	MMIODev  mmio ( IMMIODev::create( "mmio", MEM_SIZE) );
	Dev      dummy_container( IDev::create( "dummy", 0 ) );

		mmio->addAtAddress( IIntField::create("val" , 32     ), REGBASE+REG_SCR_OFF );
		mmio->addAtAddress( CMyCommandImpl::create("cmd"), 0 );

	ISequenceCommand::Items items;
		items.push_back( ISequenceCommand::Item( "../val", 0 ) );
		items.push_back( ISequenceCommand::Item( "usleep", 100000 ) );
		items.push_back( ISequenceCommand::Item( "../cmd", 0 ) );

		dummy_container->addAtAddress( ISequenceCommand::create("seq", &items) );

		mmio->addAtAddress( dummy_container, 0 );


		netio->addAtAddress( mmio, INetIODev::createPortBuilder() );

		root = netio;
	}

	if ( dmp_yaml ) {
#ifdef WITH_YAML
		CYamlFieldFactoryBase::dumpYamlFile( root, dmp_yaml, "root" );
#endif
	}

	ScalVal val( IScalVal::create( root->findByName("mmio/val") ) );

	Command cmd( ICommand::create( root->findByName("mmio/cmd") ) );

	Command seq( ICommand::create( root->findByName("mmio/dummy/seq") ) );

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

	val->setVal( 44 );
	val->getVal( &v, 1 );
	if ( v != 44 ) {
		throw TestFailed();
	}


	struct timespec then;
	clock_gettime( CLOCK_MONOTONIC, &then );

	seq->execute();

	struct timespec now;
	clock_gettime( CLOCK_MONOTONIC, &now );

	val->getVal( &v, 1 );
	if ( v != 0xdeadbeef ) {
		throw TestFailed();
	}

	if ( now.tv_nsec < then.tv_nsec ) {
		now.tv_nsec += 1000000000;
		now.tv_sec  -= 1;
	}

	uint64_t difft = (now.tv_nsec - then.tv_nsec)/1000;

	difft += (now.tv_sec - then.tv_sec) * 1000000;

	if ( difft < 100000 ) {
		throw TestFailed();
	}


} catch (CPSWError e) {
	fprintf(stderr,"CPSW Error: %s\n", e.getInfo().c_str());
	throw e;
}

	fprintf(stderr,"Command test PASSED\n");
	return 0;
}
