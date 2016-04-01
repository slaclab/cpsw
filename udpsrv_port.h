#ifndef UDPSRV_PORT_H
#define UDPSRV_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UdpPrt_ *UdpPrt;

int udpPrtRecv(UdpPrt , void *hdr, unsigned hsize, void *buf, unsigned size);
int udpPrtSend(UdpPrt , void *hdr, unsigned hsize, void *buf, unsigned size);
int udpPrtIsConn(UdpPrt);

#define WITH_RSSI    1
#define WITHOUT_RSSI 0

UdpPrt  udpPrtCreate(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, int hasRssi);
void    udpPrtDestroy(UdpPrt);

#ifdef __cplusplus
};
#endif

#endif
