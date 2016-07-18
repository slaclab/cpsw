#ifndef CPSW_NETIO_DEV_H
#define CPSW_NETIO_DEV_H

#include <cpsw_hub.h>

#include <cpsw_proto_mod.h>
#include <cpsw_proto_mod_rssi.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>
#include <string>

using std::vector;

class CNetIODevImpl;

typedef shared_ptr<CNetIODevImpl> NetIODevImpl;

struct Mutex;

class DynTimeout {
private:
	static const uint64_t AVG_SHFT  = 6; // time-constant for averager v(n+1) = v(n) + (x - v(n))/(1<<AVG_SHFT)
	static const uint64_t MARG_SHFT = 2; // timeout is avg << MARG_SHFT
	static const uint64_t CAP_US    = 5000; // empirical low-limit (under non-RT system)
	CTimeout maxRndTrip_;
	uint64_t avgRndTrip_;
	CTimeout lastUpdate_;
	CTimeout dynTimeout_;
	unsigned nSinceLast_;
	uint64_t timeoutCap_;
protected:
	void  setLastUpdate();
public:
	DynTimeout(const CTimeout &iniv);

	const CTimeout &get()  const { return  dynTimeout_; }
	const CTimeout *getp() const { return &dynTimeout_; }

	const CTimeout &getMaxRndTrip() const { return maxRndTrip_; }
	const CTimeout  getAvgRndTrip() const;

	void update(const struct timespec *now, const struct timespec *then);

	void setTimeoutCap(uint64_t cap)
	{
		timeoutCap_ = cap;
		if ( dynTimeout_.getUs() < cap )
			dynTimeout_.set( cap );
	}

	void relax();
	void reset(const CTimeout &);

};

class CCommAddressImpl : public CAddressImpl {
protected:
	ProtoPort      protoStack_;
	bool           running_;

public:
	CCommAddressImpl(AKey k, ProtoPort protoStack)
	: CAddressImpl(k),
	  protoStack_(protoStack),
      running_(false)
	{
	}

	CCommAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi, int tDest);

	CCommAddressImpl(CCommAddressImpl &orig)
	: CAddressImpl(orig)
	{
		throw InternalError("Clone not implemented"); /* need to clone protocols, ... */
	}

	ProtoPort
	getProtoStack() const
	{
		return protoStack_;
	}

	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	virtual ~CCommAddressImpl();

	virtual void startProtoStack();
	virtual void shutdownProtoStack();
};

class CSRPAddressImpl : public CCommAddressImpl {
private:            
	INetIODev::ProtocolVersion protoVersion_;
	CTimeout         usrTimeout_;
	mutable DynTimeout dynTimeout_;
	bool             useDynTimeout_;
	uint32_t         ignoreMemResp_;
	unsigned         retryCnt_;
	mutable unsigned nRetries_;
	mutable unsigned nWrites_;
	mutable unsigned nReads_;
	uint8_t          vc_;
	bool             needSwap_;
	mutable uint32_t tid_;
	uint32_t         tidMsk_;
	uint32_t         tidLsb_;
	bool             byteResolution_;
	unsigned         maxWordsRx_;
	unsigned         maxWordsTx_;

protected:
	mutable CMtx     mutex_;
	virtual uint64_t readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const;
	virtual uint64_t writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

public:
	CSRPAddressImpl(AKey key, INetIODev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc, bool useRssi, int tDest);

	CSRPAddressImpl(AKey key, INetIODev::PortBuilder, ProtoPort);

	CSRPAddressImpl(CSRPAddressImpl &orig)
	: CCommAddressImpl(orig),
	  dynTimeout_(orig.dynTimeout_.get()),
	  nRetries_(0)
	{
		throw InternalError("Clone not implemented"); /* need to clone mutex, ... */
	}
	virtual ~CSRPAddressImpl();
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	// ANY subclass must implement clone(AKey) !
	virtual CSRPAddressImpl *clone(AKey k) { return new CSRPAddressImpl( *this ); }
	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return usrTimeout_.getUs(); }
	virtual unsigned getDynTimeoutUs()                   const { return dynTimeout_.get().getUs(); }
	virtual unsigned getRetryCount()                     const { return retryCnt_;  }
	virtual INetIODev::ProtocolVersion getProtoVersion() const { return protoVersion_; }
	virtual uint8_t  getVC()                             const { return vc_; }
	virtual uint32_t getTid()                            const { return tid_ = (tid_ + tidLsb_) & tidMsk_; }
};

class CNetIODevImpl : public CDevImpl, public virtual INetIODev {
private:
	in_addr_t        d_ip_;
	std::string      ip_str_;
protected:
	CNetIODevImpl(CNetIODevImpl &orig, Key &k)
	: CDevImpl(orig, k)
	{
		throw InternalError("Cloning of CNetIODevImpl not yet implemented");
	}

	virtual ProtoPort findProtoPort(ProtoPortMatchParams *);
	friend CSRPAddressImpl::CSRPAddressImpl(IAddress::AKey, INetIODev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc, bool useRssi, int tDest);
	friend CCommAddressImpl::CCommAddressImpl(IAddress::AKey key, unsigned short dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi, int tDest);

	ProtoPort makeProtoStack(PortBuilder bldr);

public:
	CNetIODevImpl(Key &key, const char *name, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str_.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip_; }

	virtual CNetIODevImpl *clone(Key &k) { return new CNetIODevImpl( *this, k ); }

	virtual bool portInUse(unsigned port);

	virtual void addAtAddress(Field child, PortBuilder bldr);

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 1000, unsigned retryCnt = 5, uint8_t vc = 0, bool useRssi = false, int tDest = -1);
	virtual void addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi = false, int tDest = -1);

	virtual void setLocked();

	class CPortBuilder;
};

#endif
