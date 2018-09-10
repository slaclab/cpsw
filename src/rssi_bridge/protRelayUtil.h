#ifndef RSSIB_PROT_RELAY_UTIL_H
#define RSSIB_PROT_RELAY_UTIL_H

#include <printErrMsg.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc/rpc.h>

int
mkMcSd(int srv, struct sockaddr_in *sap);

CLIENT *
mkMcClnt(struct sockaddr_in *sap, unsigned long prog, unsigned long vers);

const struct sockaddr_in *
getRssibMapClntAddr();

CLIENT *
getRssibMapClnt();

CLIENT *
createRssibMapClnt(const char *group, unsigned short port);

SVCXPRT *
protRelay();

SVCXPRT *
protMap();

#define RELAY_SERVER_PORT 4567
#define MAP_MC_ADDRESS "239.0.2.3"

#ifdef __cplusplus
}
#endif

#endif
