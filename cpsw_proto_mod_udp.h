#ifndef CPSW_PROTO_MOD_UDP_H
#define CPSW_PROTO_MOD_UDP_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#include <vector>

#include <boost/shared_ptr.hpp>
using boost::shared_ptr;

class CSockSd;
typedef shared_ptr<CSockSd> SockSd;

class CSockSd {
private:
	int sd_;

	CSockSd();

public:

	virtual ~CSockSd();

	virtual int getSd() const { return sd_; }

	static SockSd create();
};

class CUdpHandlerThread {
protected:
	SockSd         sd_;
	pthread_t      tid_;
	bool           running_;

private:
	static void * threadBody(void *arg);

protected:
	virtual void threadBody() = 0;

public:
	virtual void getMyAddr(struct sockaddr_in *addr_p);

	CUdpHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me_p = NULL);

	// only start after object is fully constructed
	virtual void start();

	virtual ~CUdpHandlerThread();
};

class CUdpRxHandlerThread : public CUdpHandlerThread {
public:
	CBufQueue  *pOutputQueue_;

protected:

	virtual void threadBody();

public:
	CUdpRxHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me, CBufQueue *oqueue);
};

class CUdpPeerPollerThread : public CUdpHandlerThread {
private:
	unsigned pollSecs_;

protected:
	virtual void threadBody();

public:
	CUdpPeerPollerThread(struct sockaddr_in *dest, struct sockaddr_in *me = NULL, unsigned pollSecs = 60);
};

class CProtoModUdp : public CProtoMod {
protected:
	std::vector< CUdpRxHandlerThread > rxHandlers_;
	CUdpPeerPollerThread               poller_;
public:
	CProtoModUdp(CProtoModKey k, struct sockaddr_in *dest, CBufQueueBase::size_type depth, unsigned nThreads = 1);
	virtual const char *getName() const { return "UDP"; }

	virtual void dumpInfo(FILE *f);

	virtual ~CProtoModUdp();
};

#endif
