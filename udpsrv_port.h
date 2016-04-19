#ifndef UDPSRV_PORT_H
#define UDPSRV_PORT_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UdpPrt_ *UdpPrt;
typedef struct UdpQue_ *UdpQue;

int udpPrtRecv(UdpPrt , void *buf, unsigned size);
int udpPrtSend(UdpPrt , void *buf, unsigned size);
int udpPrtIsConn(UdpPrt);

UdpQue udpQueCreate(unsigned depth);
void   udpQueDestroy(UdpQue);
// zero timeout -> forever!
int udpQueRecv(UdpQue , void *buf, unsigned size, struct timespec *abs_timeout);
int udpQueTryRecv(UdpQue , void *buf, unsigned size);
int udpQueTrySend(UdpQue , void *buf, unsigned size);



#define WITH_RSSI    1
#define WITHOUT_RSSI 0

UdpPrt  udpPrtCreate(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, int hasRssi);
void    udpPrtDestroy(UdpPrt);

#ifdef __cplusplus
};
#endif

#endif
