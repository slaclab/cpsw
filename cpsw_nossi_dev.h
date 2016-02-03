#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

#include <cpsw_proto_mod.h>
#include <cpsw_proto_mod_srpmux.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>

using std::vector;

class CNoSsiDevImpl;

typedef shared_ptr<CNoSsiDevImpl> NoSsiDevImpl;

struct Mutex;

class DynTimeout {
private:
	CTimeout maxRndTrip_;
	CTimeout avgRndTrip_;
	CTimeout lastUpdate_;
	CTimeout dynTimeout_;
	unsigned nSinceLast_;
protected:
	void  setLastUpdate();
public:
	DynTimeout(const CTimeout &iniv);

	const CTimeout &get()  const { return  dynTimeout_; }
	const CTimeout *getp() const { return &dynTimeout_; }

	void update(const struct timespec *now, const struct timespec *then);

	void relax();
	void reset(const CTimeout &);
};

class CCommAddressImpl : public CAddressImpl {
protected:
	ProtoPort      protoStack_;

public:
	CCommAddressImpl(AKey k)
	: CAddressImpl(k)
	{
	}

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

	virtual ~CCommAddressImpl()
	{
	}
};

class CUdpSRPAddressImpl : public CCommAddressImpl {
private:            
	INoSsiDev::ProtocolVersion protoVersion_;
	unsigned short   dport_;
	CTimeout         usrTimeout_;
	mutable DynTimeout dynTimeout_;
	unsigned         retryCnt_;
	mutable unsigned nRetries_;
	uint8_t          vc_;
	bool             needSwap_;
	mutable uint32_t tid_;
	uint32_t         tidMsk_;
	uint32_t         tidLsb_;

protected:
	Mutex            *mutex_;
	virtual uint64_t readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const;
	virtual uint64_t writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;
public:
	CUdpSRPAddressImpl(AKey key, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc);
	CUdpSRPAddressImpl(CUdpSRPAddressImpl &orig)
	: CCommAddressImpl(orig),
	  dynTimeout_(orig.dynTimeout_.get()),
	  nRetries_(0)
	{
		throw InternalError("Clone not implemented"); /* need to clone mutex, ... */
	}
	virtual ~CUdpSRPAddressImpl();
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	// ANY subclass must implement clone(AKey) !
	virtual CUdpSRPAddressImpl *clone(AKey k) { return new CUdpSRPAddressImpl( *this ); }
	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return usrTimeout_.getUs(); }
	virtual unsigned getDynTimeoutUs()                   const { return dynTimeout_.get().getUs(); }
	virtual unsigned getRetryCount()                     const { return retryCnt_;  }
	virtual INoSsiDev::ProtocolVersion getProtoVersion() const { return protoVersion_; }
	virtual uint8_t  getVC()                             const { return vc_; }
	virtual unsigned short getDport()                    const { return dport_; }
	virtual uint32_t getTid()                            const { return tid_ = (tid_ + tidLsb_) & tidMsk_; }
};

class CUdpStreamAddressImpl : public CCommAddressImpl {
private:
	unsigned dport_;
protected:
	CUdpStreamAddressImpl(CUdpStreamAddressImpl &orig)
	: CCommAddressImpl( orig ),
	  dport_(orig.dport_)
	{
		throw InternalError("Clone not implemented");
	}

public:
	CUdpStreamAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads);
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	virtual CUdpStreamAddressImpl *clone(AKey k) { return new CUdpStreamAddressImpl( *this ); } /* need to clone stack */

	virtual ~CUdpStreamAddressImpl() {}
};

class CNoSsiDevImpl : public CDevImpl, public virtual INoSsiDev {
private:
	in_addr_t        d_ip_;
	string           ip_str_;
protected:
	CNoSsiDevImpl(CNoSsiDevImpl &orig, Key &k)
	: CDevImpl(orig, k)
	{
		throw InternalError("Cloning of CNoSsiDevImpl not yet implemented");
	}

	virtual ProtoPort findProtoPort(ProtoPortMatchParams *);
	friend CUdpSRPAddressImpl::CUdpSRPAddressImpl(IAddress::AKey, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc);

public:
	CNoSsiDevImpl(Key &key, const char *name, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str_.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip_; }

	virtual CNoSsiDevImpl *clone(Key &k) { return new CNoSsiDevImpl( *this, k ); }

	virtual bool portInUse(unsigned port);

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 1000, unsigned retryCnt = 5, uint8_t vc = 0);
	virtual void addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads);

	virtual void setLocked();
};

#endif
