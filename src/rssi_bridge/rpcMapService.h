#ifndef RPC_MAP_SERVER_H
#define RPC_MAP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <rpc/rpc.h>

#define MAP_PORT_DESC_FLG_RSSI (1<<0)

typedef struct PortMap_ {
	unsigned short reqPort;
	unsigned short actPort; /* filled by reply; 0 means 'no support' */
	unsigned char  flags;
} PortMap;

/* Start the RPC MAP server; returns 1 on failure,
 * 0 on success.
 *
 * The caller is supposed to call svc_run() (or similar) 
 * upon successful return.
 *
 */
SVCXPRT *
rpcMapServer(struct sockaddr_in *mcsa, in_addr_t ipAddr, PortMap *maps, unsigned num_maps);

int
rpcMapLookup(struct sockaddr_in *srva, int sd, in_addr_t ipAddr, PortMap *ports, unsigned num_ports, unsigned long timeoutUS);

/*
 * Probe/ping the relay service to obtain the multicast
 * address and port which the MAP server should use.
 *
 * If the probe fails then the process is forked, the
 * child runs a relay service and the probe is retried.
 *
 * The MC address is returned in *mcsa upon success
 * (0 return value).
 */
int
rpcProbeRelay(struct sockaddr_in *mcsa);

unsigned short
rpcRelayServerPort();

#ifdef __cplusplus
}
#endif

#endif
