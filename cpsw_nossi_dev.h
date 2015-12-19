#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

#include <cpsw_proto_mod.h>
#include <cpsw_proto_mod_depack.h>
#include <cpsw_proto_mod_udp.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <vector>

using std::vector;

class CNoSsiDevImpl;

typedef shared_ptr<CNoSsiDevImpl> NoSsiDevImpl;

struct Mutex;

class CUdpAddressImpl : public CAddressImpl {
private:            
	INoSsiDev::ProtocolVersion protoVersion_;
	unsigned short   dport_;
	int              sd_;
	unsigned         timeoutUs_;
	unsigned         retryCnt_;
	uint8_t          vc_;
	bool             needSwap_;
	mutable uint32_t tid_;
protected:
	Mutex            *mutex_;
	virtual uint64_t readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const;
	virtual uint64_t writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;
public:
	CUdpAddressImpl(AKey key, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc);
	virtual ~CUdpAddressImpl();
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	// ANY subclass must implement clone(AKey) !
	virtual CUdpAddressImpl *clone(AKey k) { throw InternalError("Clone not implemented"); } /* need to clone socket */
	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return timeoutUs_; }
	virtual unsigned getRetryCount()                     const { return retryCnt_;  }
	virtual INoSsiDev::ProtocolVersion getProtoVersion() const { return protoVersion_; }
	virtual uint8_t  getVC()                             const { return vc_; }
	virtual unsigned short getDport()                    const { return dport_; }
	virtual uint32_t getTid()                            const { return ++tid_; }
};

class CUdpStreamAddressImpl : public CAddressImpl {
private:
	unsigned dport_;
	ProtoMod protoStack_;

public:
	CUdpStreamAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize);
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	virtual CUdpStreamAddressImpl *clone(AKey k) { throw InternalError("Clone not implemented"); } /* need to clone socket */

	virtual ~CUdpStreamAddressImpl() {}
};

class CNoSsiDevImpl : public CDevImpl, public virtual INoSsiDev {
private:
	in_addr_t        d_ip_;
	string           ip_str_;
	vector<unsigned> ports_;
protected:
	virtual void addPort(unsigned port);
	CNoSsiDevImpl(CNoSsiDevImpl &orig, Key &k)
	: CDevImpl(orig, k)
	{
		throw InternalError("Cloning of CNoSsiDevImpl not yet implemented");
	}
public:
	CNoSsiDevImpl(Key &key, const char *name, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str_.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip_; }

	virtual CNoSsiDevImpl *clone(Key &k) { return new CNoSsiDevImpl( *this, k ); }

	virtual bool portInUse(unsigned port);

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 100, unsigned retryCnt = 5, uint8_t vc = 0);
	virtual void addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize);

	virtual void setLocked();
};

#endif
