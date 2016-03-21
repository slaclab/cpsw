#ifndef UDPSRV_PORT_H
#define UDPSRV_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UdpPrt_ *UdpPrt;

int udpPrtRecv(UdpPrt , void *hdr, unsigned hsize, void *buf, unsigned size);
int udpPrtSend(UdpPrt , void *hdr, unsigned hsize, void *buf, unsigned size);
int udpPrtIsConn(UdpPrt);

UdpPrt  udpPrtCreate(const char *ina, unsigned port);
void    udpPrtDestroy(UdpPrt);

#ifdef __cplusplus
};
#endif

#endif
