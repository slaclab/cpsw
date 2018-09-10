#ifndef RPC_MAP_SERVER_H
#define RPC_MAP_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start the RPC MAP server; returns 1 on failure,
 * 0 on success.
 *
 * The caller is supposed to call svc_run() (or similar) 
 * upon successful return.
 *
 * The MAP server tries to contact the RELAY server
 * in order to obtain the multicast address+port.
 * If contacting the RELAY fails then this routine
 * will attempt to start one.
 */
int
rpcMapServer();

#ifdef __cplusplus
}
#endif

#endif
