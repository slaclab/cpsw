#include <cpsw_proto_mod_rssi.h>

#include <cpsw_yaml.h>

BufChain
CProtoModRssi::pop(const CTimeout *timeout, bool abs_timeout)
{
	if ( ! timeout || timeout->isIndefinite() ) {
		return outQ_->pop( NULL );
	} else if ( timeout->isNone() ) {
		return outQ_->tryPop();
	} else if ( abs_timeout ) {
		return outQ_->pop( timeout );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPop( timeout ) );
		return outQ_->pop( &abst );
	}	
}

bool
CProtoModRssi::push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
{
	if ( ! timeout || timeout->isIndefinite() ) {
		return inpQ_->push( bc, NULL );
	} else if ( timeout->isNone() ) {
		return inpQ_->tryPush( bc );
	} else if ( abs_timeout ) {
		return inpQ_->push( bc, timeout );
	} else {
		CTimeout abst( inpQ_->getAbsTimeoutPush( timeout ) );
		return inpQ_->pop( &abst );
	}	
}

bool
CProtoModRssi::pushDown(BufChain bc, const CTimeout *rel_timeout)
{
	if ( ! rel_timeout || rel_timeout->isIndefinite() ) {
		return outQ_->push( bc, NULL );
	} else if ( rel_timeout->isNone() ) {
		return outQ_->tryPush( bc );
	} else {
		CTimeout abst( outQ_->getAbsTimeoutPush( rel_timeout ) );
		return outQ_->push( bc, &abst );
	}	
}

void
CProtoModRssi::dumpYaml(YAML::Node &node) const
{
	writeNode(node, YAML_KEY_RSSI, "TRUE");
}

BufChain
CProtoModRssi::tryPop()
{
	return outQ_->tryPop();
}

bool
CProtoModRssi::tryPush(BufChain bc)
{
	return inpQ_->tryPush( bc );
}

ProtoMod
CProtoModRssi::getProtoMod()
{
	return getSelfAs<ProtoModRssi>();
}

ProtoMod
CProtoModRssi::getUpstreamProtoMod()
{
	return upstream_ ? upstream_->getProtoMod() : ProtoMod();
}

ProtoPort
CProtoModRssi::getUpstreamPort()
{
	return upstream_;
}

IEventSource *
CProtoModRssi::getReadEventSource()
{
	return outQ_->getReadEventSource();
}
	
CTimeout
CProtoModRssi::getAbsTimeoutPop(const CTimeout *rel_timeout)
{
	return outQ_->getAbsTimeoutPop( rel_timeout );
}

CTimeout
CProtoModRssi::getAbsTimeoutPush(const CTimeout *rel_timeout)
{
	return inpQ_->getAbsTimeoutPush( rel_timeout );
}

void
CProtoModRssi::attach(ProtoPort upstream)
{
IEventSource *src = upstream->getReadEventSource();
	upstream_ = upstream;
	if ( ! src )
		printf("Upstream: %s has no event source\n", upstream->getProtoMod()->getName());
	CRssi::attach( upstream->getReadEventSource() );
}

void
CProtoModRssi::addAtPort(ProtoMod downstreamMod)
{
	downstreamMod->attach( getSelfAs<ProtoModRssi>() );
}

void
CProtoModRssi::dumpInfo(FILE *f)
{
	CRssi::dumpStats( f );
}

const char *
CProtoModRssi::getName() const
{
	return YAML_KEY_RSSI;
}

void
CProtoModRssi::modStartup()
{
CIntEventHandler eh;
EventSet         evSet = IEventSet::create();
int              connOpen;

	evSet->add( (CConnectionOpenEventSource*)this, &eh );
	threadStart();

	CTimeout relt( 2000000 /*us*/ );

	do {
		CTimeout abst(evSet->getAbsTimeout( &relt ));
		connOpen = eh.receiveEvent( evSet, true, &abst );
		if ( ! connOpen ) {
			fprintf(stderr,"Warning RSSI connection could not be opened (timeout) -- I'll keep trying\n");
//			throw IOError("RSSI connection could not be opened", ETIMEDOUT);
			break;	
		}
	} while ( ! connOpen );
}

BufChain
CProtoModRssi::tryPopUpstream()
{
	return getUpstreamPort()->tryPop();
}

int CProtoModRssi::iMatch(ProtoPortMatchParams *cmp)
{
	cmp->haveRssi_.handledBy_ = getProtoMod();
	if ( cmp->haveRssi_.doMatch_ ) {
		cmp->haveRssi_.matchedBy_ = getSelfAs<ProtoModRssi>();
		return 1;
	}
	return 0;
}

CProtoModRssi::~CProtoModRssi()
{
}

bool
CProtoModRssi::tryPushUpstream(BufChain bc)
{
	return getUpstreamPort()->tryPush( bc );
}


void
CProtoModRssi::modShutdown()
{
	threadStop();
}

ProtoModRssi
CProtoModRssi::create()
{
	return CShObj::create<ProtoModRssi>();
}
