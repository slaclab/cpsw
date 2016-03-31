#include <cpsw_api_builder.h>
#include <cpsw_mmio_dev.h>
#include <cpsw_command.h>
#include <boost/atomic.hpp>

#include <string.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <getopt.h>

#include <vector>

#include <pthread.h>

#include <yaml-cpp/yaml.h>

#include "cpsw_yaml.h"

#define VLEN 123
#define ADCL 10

#define SYNC_LIMIT 10

using boost::atomic;
using boost::memory_order_acquire;
using boost::memory_order_release;

using std::vector;

class TestFailed {};

class   IAXIVers;
typedef shared_ptr<IAXIVers> AXIVers;

class CAXIVersImpl;
typedef shared_ptr<CAXIVersImpl> AXIVersImpl;

class IAXIVers : public virtual IMMIODev {
public:
	static AXIVers create(const char *name);
};

class CAXIVersImpl : public CMMIODevImpl, public virtual IAXIVers {
public:
	CAXIVersImpl(Key &k, const char *name);
};

class CMasterResetImpl;
typedef shared_ptr<CMasterResetImpl> MasterResetImpl;

class CMasterResetImpl: public CCommandImpl {
public:
        CMasterResetImpl(Key &k, const char* name):
        CCommandImpl(k, name) {}

        virtual void executeCommand(Path pParent) const 
        {
//		AxiVersion axiv = IAxiVersion::create( IPath::create( pParent ) );
//		axiv->MasterReset();
        }
};

class CCounterResetImpl;
typedef shared_ptr<CCounterResetImpl> CounterResetImpl;

class CCounterResetImpl: public CCommandImpl {
public:
        CCounterResetImpl(Key &k, const char* name):
        CCommandImpl(k, name) {}

        virtual void executeCommand(Path pParent) const 
        {
		printf("Counter reset\n");
		uint64_t u64 = 0;
		ScalVal c = IScalVal::create( pParent->findByName( "counter" ) );
		c->setVal( &u64, 1 );
//		AxiVersion axiv = IAxiVersion::create( IPath::create( pParent ) ) );
//		axiv->CounterReset();
        }
};


AXIVers IAXIVers::create(const char *name)
{
AXIVersImpl v = CShObj::create<AXIVersImpl>(name);
Field f;
	f = CShObj::create<MasterResetImpl>("Command");
	v->CDevImpl::addAtAddress( f , 1 );
	f = CShObj::create<CounterResetImpl>("CounterReset");
	v->CDevImpl::addAtAddress( f , 1 );

	f = IIntField::create("dnaValue", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("fdSerial", 64, false, 0, IIntField::RO, 4);
	v->CMMIODevImpl::addAtAddress( f, 0x10 );
	f = IIntField::create("counter",  32, false, 0, IIntField::RW);
	v->CMMIODevImpl::addAtAddress( f, 0x24 );
	f = IIntField::create("bldStamp",  8, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f, 0x800, VLEN  );
	f = IIntField::create("scratchPad",32,true,  0);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	f = IIntField::create("bits",22,true, 4);
	v->CMMIODevImpl::addAtAddress( f,   4 );
	return v;	
}

CAXIVersImpl::CAXIVersImpl(Key &key, const char *name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}

class   IPRBS;
typedef shared_ptr<IPRBS> PRBS;

class CPRBSImpl;
typedef shared_ptr<CPRBSImpl> PRBSImpl;

class IPRBS : public virtual IMMIODev {
public:
	static PRBS create(const char *name);
};

class CPRBSImpl : public CMMIODevImpl, public virtual IPRBS {
public:
	CPRBSImpl(Key &k, const char *name);
};

PRBS IPRBS::create(const char *name)
{
PRBSImpl v = CShObj::create<PRBSImpl>(name);
Field f;
	f = IIntField::create("axiEn",        1, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("trig",         1, false, 1);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("busy",         1, false, 2, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("overflow",     1, false, 3, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("oneShot",      1, false, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x00 );
	f = IIntField::create("packetLength",32, false, 4);
	v->CMMIODevImpl::addAtAddress( f , 0x04 );
	f = IIntField::create("tDest",        8, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x08 );
	f = IIntField::create("tId",          8, false, 0);
	v->CMMIODevImpl::addAtAddress( f , 0x09 );
	f = IIntField::create("dataCnt",     32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x0c );
	f = IIntField::create("eventCnt",    32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x10 );
	f = IIntField::create("randomData",  32, false, 0, IIntField::RO);
	v->CMMIODevImpl::addAtAddress( f , 0x14 );

	return v;
}

CPRBSImpl::CPRBSImpl(Key &key, const char *name) : CMMIODevImpl(key, name, 0x1000, LE)
{
}

static ScalVal_RO vpb(vector<ScalVal_RO> *v, ScalVal_RO x)
{
	v->push_back( x );
	return x;
}

static ScalVal vpb(vector<ScalVal_RO> *v, ScalVal x)
{
	v->push_back( x );
	return x;
}

class ThreadArg {
private:
	atomic<int> firstFrame_;
	atomic<int> nFrames_;
public:
	ThreadArg()
	:firstFrame_(-1),
	 nFrames_(0)
	{
	}

	void gotFrame(int frameNo)
	{
		int expected = -1;
		firstFrame_.compare_exchange_strong( expected, frameNo );
		nFrames_.fetch_add(1, memory_order_release);
	}

	int firstFrame()
	{
		return firstFrame_.load( memory_order_acquire );
	}

	int nFrames()
	{
		return nFrames_.load( memory_order_acquire );
	}
};


static void *rxThread(void *arg)
{
Stream strm = IStream::create( IPath::create("/fpga/dataSource") );

uint8_t  buf[16];
int64_t  got;
ThreadArg *stats = static_cast<ThreadArg*>(arg);

	while ( 1 ) {
		got = strm->read( buf, sizeof(buf), CTimeout(20000000) );
		if ( ! got ) {
			fprintf(stderr,"RX thread timed out\n");
			exit (1);
		}
		unsigned frameNo;
		if ( got > 2 ) {
			frameNo = (buf[1]<<4) | (buf[0] >> 4);
			stats->gotFrame( frameNo );
			printf("Received frame # %d\n", frameNo);
		} else {
			fprintf(stderr,"Received frame too small!\n");
		}
	}

	return 0;
}

static void usage(const char *nm)
{
	fprintf(stderr,"Usage: %s [-a <ip_addr>] [-mh] [-V <version>] [-S <length>] [-n <shots>] [-p <period>] [-f <file>]\n", nm);
	fprintf(stderr,"       -a <ip_addr>:  destination IP\n");
	fprintf(stderr,"       -V <version>:  SRP version (1 or 2)\n");
	fprintf(stderr,"       -m          :  use 'fake' memory image instead\n");
	fprintf(stderr,"                      of real device and UDP\n");
	fprintf(stderr,"       -S <length> :  test streaming interface\n");
	fprintf(stderr,"                      with frames of 'length'.\n");
	fprintf(stderr,"                      'length' must be > 0 to enable streaming\n");
	fprintf(stderr,"       -n <shots>  :  stream 'shots' fragments\n");
	fprintf(stderr,"                      (defaults to 10).\n");
	fprintf(stderr,"       -p <period> :  trigger a fragment every <period> ms\n");
	fprintf(stderr,"                      (defaults to 1000).\n");
	fprintf(stderr,"       -f <file>   :  yaml file describing axiv object\n");
	fprintf(stderr,"       -h          :  print this message\n");
}

int
main(int argc, char **argv)
{
int         rval    = 0;
const char *ip_addr = "192.168.2.20";
bool        use_mem = false;
int        *i_p;
int         vers    = 2;
int         length  = 0;
int         shots   = 10;
int         period  = 1000; // ms
YAML::Node doc;


	for ( int opt; (opt = getopt(argc, argv, "a:mV:S:hn:p:f:")) > 0; ) {
		i_p = 0;
		switch ( opt ) {
			case 'a': ip_addr = optarg;  break;
			case 'm': use_mem = true;    break;
			case 'V': i_p     = &vers;   break;
			case 'S': i_p     = &length; break;
			case 'n': i_p     = &shots;  break;
			case 'p': i_p     = &period; break;
			case 'f': doc = YAML::LoadFile(optarg); break;
			case 'h': usage(argv[0]); return 0;
			default:
				fprintf(stderr,"Unknown option '%c'\n", opt);
				usage(argv[0]);
				throw TestFailed();
		}
		if ( i_p && 1 != sscanf(optarg, "%i", i_p) ) {
			fprintf(stderr,"Unable to scan argument to option '-%c'\n", opt);
			throw TestFailed();
		}
	}

	if ( vers != 1 && vers != 2 ) {
		fprintf(stderr,"Invalid protocol version '%i' -- must be 1 or 2\n", vers);
		throw TestFailed();
	}

try {

NoSsiDev  root = INoSsiDev::create("fpga", ip_addr);
MemDev    rmem = IMemDev::create  ("rmem", 0x100000);
MMIODev mmio; /* can be generated if file name is passed in */

if( !doc.IsNull() ) {
	mmio = doc.as<MMIODev>();
}
else {
	mmio = IMMIODev::create ("mmio",0x100000);

	AXIVers   axiv = IAXIVers::create ("vers");
	MMIODev   sysm = IMMIODev::create ("sysm",0x1000, LE);
	PRBS      prbs = IPRBS::create    ("prbs");

	sysm->addAtAddress( IIntField::create("adcs", 16, true, 0), 0x400, ADCL, 4 );

	mmio->addAtAddress( axiv, 0x00000 );
	mmio->addAtAddress( sysm, 0x10000 );
	mmio->addAtAddress( prbs, 0x30000 );
}

uint8_t str[VLEN];
int16_t adcv[ADCL];
uint64_t u64;
uint32_t u32;
uint16_t u16;

	rmem->addAtAddress( mmio );

	uint8_t *buf = rmem->getBufp();
	for (int i=16; i<24; i++ )
		buf[i]=i-16;

	if ( use_mem )
		length = 0;

	root->addAtAddress( mmio, 1 == vers ? INoSsiDev::SRP_UDP_V1 : INoSsiDev::SRP_UDP_V2, 8192, 500 /*us*/ );

	if ( length > 0 )
		root->addAtStream( IField::create("dataSource"), 8193, 10000000 /* us */ );

	IDev::getRootDev()->addAtAddress( root );

	// can use raw memory for testing instead of UDP
	Path pre = use_mem ? IPath::create( rmem ) : IPath::create( root );
	/*
        *Command cmd = ICommand::create( pre->findByName("mmio/vers/Command") );
        *cmd->execute();
        *Command counterRst = ICommand::create( pre->findByName("mmio/vers/CounterReset") );
        *counterRst->execute();
	*/
	ScalVal_RO bldStamp = IScalVal_RO::create( pre->findByName("mmio/vers/bldStamp") );
	ScalVal_RO fdSerial = IScalVal_RO::create( pre->findByName("mmio/vers/fdSerial") );
	ScalVal_RO dnaValue = IScalVal_RO::create( pre->findByName("mmio/vers/dnaValue") );
	ScalVal_RO counter  = IScalVal_RO::create( pre->findByName("mmio/vers/counter" ) );
	ScalVal  scratchPad = IScalVal::create   ( pre->findByName("mmio/vers/scratchPad") );
	ScalVal        bits = IScalVal::create   ( pre->findByName("mmio/vers/bits") );

	vector<ScalVal_RO> vals;

	ScalVal    axiEn        = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/axiEn") ));
	ScalVal    trig         = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/trig") ));
	ScalVal_RO busy         = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/prbs/busy") ));
	ScalVal_RO overflow     = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/prbs/overflow") ));
	ScalVal    oneShot      = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/oneShot") ));
	ScalVal    packetLength = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/packetLength") ));
	ScalVal    tDest        = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/tDest") ));
	ScalVal    tId          = vpb(&vals, IScalVal::create   ( pre->findByName("mmio/prbs/tId") ));
	ScalVal_RO dataCnt      = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/prbs/dataCnt") ));
	ScalVal_RO eventCnt     = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/prbs/eventCnt") ));
	ScalVal_RO randomData   = vpb(&vals, IScalVal_RO::create( pre->findByName("mmio/prbs/randomData") ));


	ScalVal_RO adcs = IScalVal_RO::create( pre->findByName("mmio/sysm/adcs") );

	bldStamp->getVal( str, sizeof(str)/sizeof(str[0]) );

	printf("Build String:\n%s\n", (char*)str);
	fdSerial->getVal( &u64, 1 );
	printf("Serial #: 0x%"PRIx64"\n", u64);
	dnaValue->getVal( &u64, 1 );
	printf("DNA    #: 0x%"PRIx64"\n", u64);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%"PRIx32"\n", u32);
	counter->getVal( &u32, 1 );
	printf("Counter : 0x%"PRIx32"\n", u32);

	adcs->getVal( (uint16_t*)adcv, sizeof(adcv)/sizeof(adcv[0]) );
	printf("\n\nADC Values:\n");
	for ( int i=0; i<ADCL; i++ ) {
		printf("  %6hd\n", adcv[i]);
	}

	u16=0x6765;
	u32=0xffffffff;
	scratchPad->setVal( &u32, 1 );
	bits->setVal( &u16, 1 );
	scratchPad->getVal( &u32, 1 );
	printf("ScratchPad: 0x%"PRIx32"\n", u32);

	if ( u32 == 0xfc06765f ) {
		printf("Readback of merged bits (expected 0xfc06765f) PASSED\n");
	} else {
		printf("Readback of merged bits (expected 0xfc06765f) FAILED\n");
		throw TestFailed();	
	}

	if ( length > 0 ) {
		printf("PRBS Regs:\n");
		uint32_t v;
		for (unsigned i=0; i<vals.size(); i++ ) {
			vals[i]->getVal( &v, 1 );
			printf("%14s: %d\n", vals[i]->getName(), v );
		}
		v = 1;
		axiEn->setVal( &v, 1 );
		v = length;
		packetLength->setVal( &v, 1 );
		v = 1;
		oneShot->setVal( &v, 1 );

		pthread_t tid;
		void     *ign;
		struct timespec p;
		ThreadArg arg;
		p.tv_sec  = period/1000;
		p.tv_nsec = (period % 1000) * 1000000;
		if ( pthread_create( &tid, 0, rxThread, &arg ) ) {
			perror("pthread_create");
		}
		for (int i = 0; i<shots; i++) {
			struct timespec dly = p;
			nanosleep( &dly, 0 );
			oneShot->setVal( 1 );
			// not truly thread safe but
			// a correct algorithm would
			// be much more complex.
			if ( arg.firstFrame() < 0 ) {
				//not yet synchronized
				if ( i > SYNC_LIMIT ) {
					fprintf(stderr,"Stream unable to synchronize: FAILED\n");
					rval = 1;
					break;
				}
				shots++;
			}
		}
		pthread_cancel( tid );
		pthread_join( tid, &ign );
		printf("%d shots were fired; %d frames received\n", shots, arg.nFrames());
		printf("(Difference to requested shots are due to synchronization)\n");
	}

	// try to get better statistics for the average timeout
	for (int i=0; i<1000; i++ )
		counter->getVal( &u32, 1 );

	root->findByName("mmio")->tail()->dump(stdout);

} catch (CPSWError &e) {
	printf("CPSW Error: %s\n", e.getInfo().c_str());
	throw;
}

	return rval;
}
