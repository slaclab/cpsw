/*
 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
*/

#ifndef UDPSRV_PORT_H
#define UDPSRV_PORT_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IoPrt_ *IoPrt;
typedef struct IoQue_ *IoQue;

int ioPrtRecv(IoPrt , void *buf, unsigned size, struct timespec *abs_timeout);
int ioPrtSend(IoPrt , void *buf, unsigned size);
int ioPrtIsConn(IoPrt);
int ioPrtRssiIsConn(IoPrt prt);

IoQue ioQueCreate(unsigned depth);
void   ioQueDestroy(IoQue);
// zero timeout -> forever!
int ioQueRecv(IoQue , void *buf, unsigned size, struct timespec *abs_timeout);
int ioQueTryRecv(IoQue , void *buf, unsigned size);
int ioQueTrySend(IoQue , void *buf, unsigned size);



#define WITH_RSSI    1
#define WITHOUT_RSSI 0

IoPrt  udpPrtCreate(const char *ina, unsigned port, unsigned simLossPercent, unsigned ldScrambler, int hasRssi);
IoPrt  tcpPrtCreate(const char *ina, unsigned port);
void   ioPrtDestroy(IoPrt);

#ifdef __cplusplus
};
#endif

#endif
