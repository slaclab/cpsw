#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

class CNoSsiDevImpl;

typedef shared_ptr<CNoSsiDevImpl> NoSsiDevImpl;

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
public:
	CUdpAddressImpl(AKey key, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc);
	virtual ~CUdpAddressImpl();
	uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
	virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

	// ANY subclass must implement clone(AKey) !
	virtual CUdpAddressImpl *clone(AKey k) { return new CUdpAddressImpl( *this ); }
	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return timeoutUs_; }
	virtual unsigned getRetryCount()                     const { return retryCnt_;  }
	virtual INoSsiDev::ProtocolVersion getProtoVersion() const { return protoVersion_; }
	virtual uint8_t  getVC()                             const { return vc_; }
	virtual unsigned short getDport()                    const { return dport_; }
	virtual uint32_t getTid()                            const { return ++tid_; }
};

class CNoSsiDevImpl : public CDevImpl, public virtual INoSsiDev {
private:
	in_addr_t d_ip_;
	string    ip_str_;
public:
	CNoSsiDevImpl(FKey key, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str_.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip_; }

	virtual CNoSsiDevImpl *clone(FKey k) { return new CNoSsiDevImpl( *this ); }

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 100, unsigned retryCnt = 5, uint8_t vc = 0);
};

#endif
