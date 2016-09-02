 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

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
