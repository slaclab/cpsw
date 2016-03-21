#include <udpsrv_util.h>
#include <udpsrv_port.h>
#include <udpsrv_rssi_port.h>

struct UdpPrt_ {
	UdpPort   udp_;
	ProtoPort top_;
	CMtx      mtx_;
	UdpPrt_(const char *mtxnm):mtx_(mtxnm){}
};

UdpPrt udpPrtCreate(const char *ina, unsigned port, int withRssi)
{
UdpPrt p = new UdpPrt_("udp_port");
	p->udp_ = IUdpPort::create(ina, port);
	if ( withRssi ) {
		p->top_ = CRssiPort::create( true );
		p->top_->attach( p->udp_ );
	} else {
		p->top_ = p->udp_;
	}
	return p;
}

void udpPrtDestroy(UdpPrt p)
{
	p->top_.reset();
	p->udp_.reset();
	delete p;
}

int udpPrtRecv(UdpPrt p, void *hdr, unsigned hsize, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = p->top_->pop( NULL );
	if ( ! bc )
		return -1;

unsigned bufsz = bc->getSize();

	if ( hsize > 0 ) {
		if ( hsize > bufsz )
			hsize = bufsz;
		bc->extract(hdr, 0, hsize);
	}

	if ( hsize + size > bufsz )
		size = bufsz - hsize;

	bc->extract(buf, hsize, size);

	return hsize + size;
}

int udpPrtIsConn(UdpPrt p)
{
CMtx::lg( &p->mtx_ );
	return p->udp_->isConnected();
}

int udpPrtSend(UdpPrt p, void *hdr, unsigned hsize, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = IBufChain::create();
Buf      b  = bc->createAtHead( IBuf::CAPA_ETH_BIG );

	if ( hsize > 0 )
		bc->insert( hdr, 0, hsize );

	bc->insert(buf, hsize, size);
	return p->top_->push(bc, NULL) ? hsize + size : -1;
}
