 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_PROTO_MOD_TCP_H
#define CPSW_PROTO_MOD_TCP_H

#include <cpsw_buf.h>
#include <cpsw_proto_mod.h>
#include <cpsw_thread.h>
#include <cpsw_sock.h>
#include <cpsw_mutex.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>
using boost::shared_ptr;
using boost::atomic;

class CProtoModTcp;
typedef shared_ptr<CProtoModTcp> ProtoModTcp;

// TCP transport for framed traffic. Frames are prepended a 32-bit
// 'length' header in network byte order.

class CProtoModTcp : public CProtoMod {
protected:

	class CRxHandlerThread : public CRunnable {
		private:
			int              sd_;
			atomic<uint64_t> nOctets_;
			atomic<uint64_t> nDgrams_;
		public:
			// cannot use smart pointer here because CProtoModUdp's
			// constructor creates the threads (and a smart ptr is
			// not available yet).
			// In any case - the thread objects are only used
			// internally...
			CProtoModTcp   *owner_;

		protected:

			virtual void* threadBody();

		public:
			CRxHandlerThread(const char *name, int threadPriority, int sd, CProtoModTcp *owner);
			CRxHandlerThread(CRxHandlerThread &orig, int sd, CProtoModTcp *owner);

			virtual uint64_t getNumOctets() { return nOctets_.load( boost::memory_order_relaxed ); }
			virtual uint64_t getNumDgrams() { return nDgrams_.load( boost::memory_order_relaxed ); }

			virtual ~CRxHandlerThread() { threadStop(); }
	};

private:
	struct sockaddr_in dest_;
	struct sockaddr_in via_;
	CSockSd            sd_;
	atomic<uint64_t>   nTxOctets_;
	atomic<uint64_t>   nTxDgrams_;
	CMtx               txMtx_;

protected:
	CRxHandlerThread  *rxHandler_;

	void createThread(int threadPriority);

	virtual bool doPush(BufChain bc, bool wait, const CTimeout *timeout, bool abs_timeout);

	virtual bool push(BufChain bc, const CTimeout *timeout, bool abs_timeout)
	{
		return doPush(bc, true, timeout, abs_timeout);
	}

	virtual bool tryPush(BufChain bc)
	{
		return doPush(bc, false, NULL, true);
	}

	virtual int iMatch(ProtoPortMatchParams *cmp);

public:
	CProtoModTcp(
		Key                      &k,
		const struct sockaddr_in *dest,
		unsigned                  depth,
		int                       threadPriority,
		const LibSocksProxy      *proxy,
		const struct sockaddr_in *via
	);

	CProtoModTcp(CProtoModTcp &orig, Key &k);

	virtual const char *getName()  const
{
		return "TCP";
	}

	virtual unsigned getDestPort() const
	{
		return ntohs( dest_.sin_port );
	}

	virtual void dumpInfo(FILE *f);

	virtual CProtoModTcp *clone(Key &k)
	{
		return new CProtoModTcp( *this, k );
	}

	virtual unsigned getMTU();

	virtual uint64_t getNumTxOctets() { return nTxOctets_.load( boost::memory_order_relaxed ); }
	virtual uint64_t getNumTxDgrams() { return nTxDgrams_.load( boost::memory_order_relaxed ); }
	virtual uint64_t getNumRxOctets();
	virtual uint64_t getNumRxDgrams();
	virtual void modStartup();
	virtual void modShutdown();

	virtual void dumpYaml(YAML::Node &) const;

	virtual ~CProtoModTcp();

};

#endif
