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
#include <unistd.h>

#include <protRelayUtil.h>

int
rpcMapLookup(in_addr_t ipAddr, unsigned short *ports, unsigned num_ports, unsigned long timeoutUS)
{
CLIENT             *clnt = 0;
struct timeval      tout;

int                 stat;
struct sockaddr_in  srvr;
int                 sd;
int                 result;
PortDesc            d[num_ports];
MapReq              req;
MapReq              res;
int                 rval = 1;
int                 i;

	req.ipAddr          = ntohl( ipAddr );
	res.ports.ports_len = num_ports;
	res.ports.ports_val = num_ports ? ports : 0;

	srvr.sin_family      = AF_INET;
	srvr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	srvr.sin_port        = htons( RELAY_SERVER_PORT );

	if ( (sd = socket( AF_INET, SOCK_STREAM, 0 )) < 0 ) {
		perror("Creating socket(SOCK_STREAM)");
		goto bail;
	}

	if ( connect(sd, (struct sockaddr*)&srvr, sizeof(srvr)) ) {
		perror("Unable to connect to server");
		goto bail;
	}

	if ( ! (clnt = clnttcp_create( &srvr, RSSIB_REL, RSSIB_REL_V0, &sd, 0, 0)) ) {
		clnt_pcreateerror("127.0.0.1");
		fprintf(stderr,"\nERROR: unable to create RELAY client\n");
		goto bail;
	}

	tout.tv_sec  = timeoutUS/1000000;
	tout.tv_usec = timeoutUS - tout.tv_sec*1000000;

	stat = clnt_call( clnt, RSSIB_REL_LKUP,
	                  (xdrproc_t)xdr_MapReq,  (caddr_t)&req,
	                  (xdrproc_t)xdr_MapReq , (caddr_t)&res,
	                  tout );


	if ( RPC_SUCCESS != stat ) {
		clnt_perrno(stat);
		fprintf(stderr,"\nERROR: RPC RSSIB_REL_LKUP failed\n");

		fprintf(stderr, "RESP %p\n", res.ports.ports_val);
		goto bail;
	}

	if ( res.ports.ports_len < num_ports ) {
		num_ports = res.ports.ports_len;
	}

	for ( i=0; i<num_ports; i++ ) {
		ports[i] = (unsigned short)res->ports.ports_val[i];
	}

	if ( ! clnt_freeres( clnt, (xdrproc_t)xdr_MapReq, (caddr_t)&res ) ) {
		fprintf(stderr,"ERROR: unable to free results!\n");
		goto bail;
	}

	rval = 0;

bail:

	if ( clnt ) {
		clnt_destroy( clnt );
	}

	if ( sd >= 0 ) {
		close( sd );
	}

	return rval;
}
