#ifndef RSSIB_PROT_RELAY_UTIL_H
#define RSSIB_PROT_RELAY_UTIL_H

#include "printErrMsg.h"
#include "rpcMapService.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc/rpc.h>
#include <netinet/in.h>

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
protMap(struct sockaddr_in *mcAddr);

#define RELAY_SERVER_PORT 8017
#define MAP_MC_ADDRESS    "239.0.2.3"

void
setPortMaps(in_addr_t ipAddr, const PortMap *portMaps, unsigned numMaps);

#ifdef __cplusplus
}
#endif

#endif
