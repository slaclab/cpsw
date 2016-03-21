#include <q.h>
#include <udpsrv_port.h>

struct UdpPrt_ {
	UdpPort port_;
	CMtx    mtx_;
	UdpPrt_(const char *mtxnm):mtx_(mtxnm){}
};

UdpPrt udpPrtCreate(const char *ina, unsigned port)
{
UdpPrt p = new UdpPrt_("udp_port");
	p->port_ = IUdpPort::create(ina, port);
	return p;
}

void udpPrtDestroy(UdpPrt p)
{
	p->port_.reset();
	delete p;
}

int udpPrtRecv(UdpPrt p, void *hdr, unsigned hsize, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = p->port_->pop( NULL );
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
	return p->port_->isConnected();
}

int udpPrtSend(UdpPrt p, void *hdr, unsigned hsize, void *buf, unsigned size)
{
CMtx::lg( &p->mtx_ );

BufChain bc = IBufChain::create();
Buf      b  = bc->createAtHead( IBuf::CAPA_ETH_BIG );

	if ( hsize > 0 )
		bc->insert( hdr, 0, hsize );

	bc->insert(buf, hsize, size);
	return p->port_->push(bc, NULL) ? hsize + size : -1;
}
