/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "prot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include <protRelayUtil.h>

#include "protMap_svc.c"

SVCXPRT *
protMap()
{
SVCXPRT            *transp = 0;
SVCXPRT            *rval   = 0;
CLIENT             *clnt   = 0;
struct timeval      tout;

int                 stat;
const char         *srvr = "127.0.0.1";
int                 mcsd;
struct sockaddr_in  srva;
struct sockaddr_in  mcsa;
struct McAddr       mca;
int                 sd;

	srva.sin_family      = AF_INET;
	srva.sin_addr.s_addr = inet_addr( srvr );
	srva.sin_port        = htons( RELAY_SERVER_PORT );

	sd                   = RPC_ANYSOCK;

	if ( ! (clnt = clnttcp_create( &srva, RSSIB_REL, RSSIB_REL_V0, &sd, 0, 0 )) ) {
		clnt_pcreateerror( srvr );
		fprintf(stderr,"\nERROR: unable to create MAP client\n");
		goto bail;
	}

	tout.tv_sec  = 1;
	tout.tv_usec = 0;

	stat = clnt_call( clnt, RSSIB_REL_GETMCPORT,
	                  (xdrproc_t)xdr_void,    (caddr_t)0,
	                  (xdrproc_t)xdr_McAddr , (caddr_t)&mca,
	                  tout );

	if ( RPC_SUCCESS != stat ) {
		clnt_perrno(stat);
		fprintf(stderr,"\nERROR: RPC RSSIB_REL_GETMCPORT failed\n");
		goto bail;
	}

	mcsa.sin_family      = AF_INET;
	mcsa.sin_port        = htons( mca.portno );
	mcsa.sin_addr.s_addr = htonl( mca.ipaddr );

#ifdef PROT_MAP_DEBUG
	printf("Remote port for multicast: %hu\n", mca.portno);
	printf("Remote addr for multicast: %s\n",  inet_ntoa( mcsa.sin_addr ));
#endif

	if ( ! clnt_freeres( clnt, (xdrproc_t)xdr_McAddr, (caddr_t)&mca ) ) {
		fprintf(stderr, "ERROR: unable to free results!\n");
		goto bail;
	}

	clnt_destroy( clnt );
	clnt = 0;

	if ( (mcsd = mkMcSd(1, &mcsa)) < 0 ) {
		fprintf(stderr, "ERROR: Unable to create multicast socket for SVC\n");
		goto bail;
	}

	transp = svcudp_create( mcsd );
	if ( ! transp ) {
		fprintf(stderr, "Unable to create UDP MAP RPC service\n");
		goto bail;
	}

	if ( ! svc_register(transp, RSSIB_MAP, RSSIB_MAP_V0, rssib_map_0, 0) ) {
		fprintf(stderr, "Unable to register RSSIB_MAP(V0) service\n");
		goto bail;
	}

	rval   = transp;
	transp = 0;

bail:
	if ( clnt ) {
		clnt_destroy( clnt );
	}

	if ( transp ) {
		svc_destroy( transp );
	}

	return rval;
}
