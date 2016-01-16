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

class CProtoModUdp;
typedef shared_ptr<CProtoModUdp> ProtoModUdp;

class CSockSd {
private:
	int sd_;
	CSockSd & operator=(CSockSd &orig) { throw InternalError("Must not assign"); }

public:

	CSockSd();

	CSockSd(CSockSd &orig);

	virtual void getMyAddr(struct sockaddr_in *addr_p);

	virtual ~CSockSd();

	virtual int getSd() const { return sd_; }

};

class CUdpHandlerThread {
protected:
	CSockSd        sd_;
	pthread_t      tid_;
	bool           running_;

private:
	static void * threadBody(void *arg);


protected:
	virtual void threadBody() = 0;

public:
	virtual void getMyAddr(struct sockaddr_in *addr_p)
	{
		sd_.getMyAddr( addr_p );
	}

	CUdpHandlerThread(struct sockaddr_in *dest, struct sockaddr_in *me_p = NULL);
	CUdpHandlerThread(CUdpHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me_p);

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
	CUdpRxHandlerThread(CUdpRxHandlerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me, CBufQueue *oqueue);
};

class CUdpPeerPollerThread : public CUdpHandlerThread {
private:
	unsigned pollSecs_;

protected:
	virtual void threadBody();

public:
	CUdpPeerPollerThread(struct sockaddr_in *dest, struct sockaddr_in *me = NULL, unsigned pollSecs = 60);
	CUdpPeerPollerThread(CUdpPeerPollerThread &orig, struct sockaddr_in *dest, struct sockaddr_in *me);
};

class CProtoModUdp : public CProtoMod {
private:
	struct sockaddr_in dest_;
	CSockSd            tx_;
protected:
	std::vector< CUdpRxHandlerThread * > rxHandlers_;
	CUdpPeerPollerThread                 *poller_;

	void spawnThreads(unsigned nRxThreads, int pollSecons);

	virtual void doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout);

	virtual void push(BufChain bc, const CTimeout *timeout, bool abs_timeout)	
	{
		doPush(bc, true, timeout, abs_timeout);
	}

	virtual void tryPush(BufChain bc)
	{
		doPush(bc, false, NULL, true);
	}

public:
	// negative or zero 'pollSecs' avoids creating a poller thread
	CProtoModUdp(Key &k, struct sockaddr_in *dest, CBufQueueBase::size_type depth, unsigned nRxThreads = 1, int pollSecs = 4);

	CProtoModUdp(CProtoModUdp &orig, Key &k);

	virtual const char *getName() const { return "UDP"; }

	virtual void dumpInfo(FILE *f);

	virtual CProtoModUdp *clone(Key &k) { return new CProtoModUdp( *this, k ); }

	virtual ~CProtoModUdp();
};

#endif
