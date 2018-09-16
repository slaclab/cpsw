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

#ifndef SIG_PF
#define SIG_PF void(*)(int)
#endif

void
rssib_map_0(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		struct MapReq rssib_map_lkup_0_arg;
	} argument;
	union {
		struct MapReq rssib_map_lkup_0_res;
	} result;
	bool_t retval;
	xdrproc_t _xdr_argument, _xdr_result;
	bool_t (*local)(char *, void *, struct svc_req *);

	switch (rqstp->rq_proc) {
	case RSSIB_MAP_ECHO:
		_xdr_argument = (xdrproc_t) xdr_void;
		_xdr_result = (xdrproc_t) xdr_void;
		local = (bool_t (*) (char *, void *,  struct svc_req *))rssib_map_echo_0_svc;
		break;

	case RSSIB_MAP_LKUP:
		_xdr_argument = (xdrproc_t) xdr_MapReq;
		_xdr_result = (xdrproc_t) xdr_MapReq;
		local = (bool_t (*) (char *, void *,  struct svc_req *))rssib_map_lkup_0_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}
	retval = (bool_t) (*local)((char *)&argument, (void *)&result, rqstp);
	if (retval > 0 && !svc_sendreply(transp, (xdrproc_t) _xdr_result, (char *)&result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		fprintf (stderr, "%s", "unable to free arguments");
		exit (1);
	}
	if (!rssib_map_0_freeresult (transp, _xdr_result, (caddr_t) &result))
		fprintf (stderr, "%s", "unable to free results");

	return;
}