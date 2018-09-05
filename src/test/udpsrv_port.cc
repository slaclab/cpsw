 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <udpsrv_util.h>
#include <udpsrv_port.h>
#include <udpsrv_rssi_port.h>

struct IoPrt_ {
	ProtoPort prt_;
	ProtoPort top_;
	CMtx      rmtx_;
	CMtx      tmtx_;
	int       hasConn_;
	IoPrt_(const char *mtxnm):rmtx_(mtxnm),tmtx_(mtxnm){}
};

int ioPrtRssiIsConn(IoPrt prt)
{
	return prt->hasConn_ && prt->top_->isConnected();
}

IoPrt udpPrtCreate(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, int withRssi)
{
ProtoPort  prt;
IoPrt     p = new IoPrt_("udp_port");

	p->prt_      = IUdpPort::create(ina, port, simLossPercent, ldScrambler);
    p->hasConn_  = (WITH_RSSI == withRssi);
	if ( withRssi ) {
		p->top_ = CRssiPort::create( true );
		p->top_->attach( p->prt_ );
	} else {
		p->top_ = p->prt_;
	}
	for ( prt = p->top_; prt; prt = prt->getUpstreamPort() )
		prt->start();

	return p;
}

IoPrt tcpPrtCreate(const char *ina, unsigned port)
{
ProtoPort  prt;
IoPrt     p = new IoPrt_("tcp_port");
	p->prt_      = ITcpPort::create(ina, port);
    p->hasConn_  = 1;
	p->top_      = p->prt_;
	for ( prt = p->top_; prt; prt = prt->getUpstreamPort() )
		prt->start();
	return p;
}

void ioPrtDestroy(IoPrt p)
{
ProtoPort  prt;

	if ( ! p )
		return;

	for ( prt = p->top_; prt; prt = prt->getUpstreamPort() )
		prt->stop();

	p->top_.reset();
	p->prt_.reset();
	delete p;
}

int xtrct(BufChain bc, void *buf, unsigned size)
{
unsigned bufsz = bc->getSize();

	if ( size > bufsz )
		size = bufsz;

	bc->extract(buf, 0, size);

	return size;
}

int ioPrtRecv(IoPrt p, void *buf, unsigned size, struct timespec *abs_timeout)
{
CMtx::lg guard( &p->rmtx_ );

BufChain bc;
	if ( abs_timeout ) {
		CTimeout to( *abs_timeout );
		bc = p->top_->pop( &to );
	} else {
		bc = p->top_->pop( NULL );
	}
	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int ioPrtIsConn(IoPrt p)
{
CMtx::lg guard( &p->rmtx_ );
	return p->prt_->isConnected();
}

static BufChain fill(void *buf, unsigned size)
{
BufChain bc = IBufChain::create();
Buf      b  = bc->createAtHead( IBuf::CAPA_ETH_BIG );

	bc->insert(buf, 0, size);

	return bc;
}

int ioPrtSend(IoPrt p, void *buf, unsigned size)
{
CMtx::lg guard( &p->tmtx_ );

BufChain bc = fill(buf, size);

	return p->top_->push(bc, NULL) ? size : -1;
}

struct IoQue_ {
	BufQueue q_;
};

IoQue ioQueCreate(unsigned depth)
{
IoQue rval = new IoQue_();
	rval->q_ = IBufQueue::create(depth);
	return rval;
}

void   ioQueDestroy(IoQue q)
{
	if ( q ) {
		q->q_.reset();
		delete(q);
	}
}

int ioQueTryRecv(IoQue q, void *buf, unsigned size)
{
BufChain bc = q->q_->tryPop();
	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int ioQueRecv(IoQue q, void *buf, unsigned size, struct timespec *abs_timeout)
{
CTimeout to( abs_timeout ? *abs_timeout : TIMEOUT_INDEFINITE );

BufChain bc = q->q_->pop( to.isIndefinite() ? NULL : &to );

	if ( ! bc )
		return -1;

	return xtrct(bc, buf, size);
}

int ioQueSend(IoQue q, void *buf, unsigned size, struct timespec *abs_timeout)
{
CTimeout to( abs_timeout ? *abs_timeout : TIMEOUT_INDEFINITE );
BufChain bc = fill(buf, size);
	return q->q_->push( bc, to.isIndefinite() ? NULL : &to )  ? size : -1;
}

int ioQueTrySend(IoQue q, void *buf, unsigned size)
{
BufChain bc = fill(buf, size);
	return q->q_->tryPush( bc ) ? size : -1;
}
