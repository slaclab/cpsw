#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

class NoSsiDevImpl;

class UdpAddressImpl : public AddressImpl {
private:            
	INoSsiDev::ProtocolVersion protoVersion;
	unsigned short   dport;
	int              sd;
	unsigned         timeoutUs;
	unsigned         retryCnt;
	uint8_t          vc;
	bool             needSwap;
	mutable uint32_t tid;
public:
	UdpAddressImpl(AKey key, INoSsiDev::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc);
	virtual ~UdpAddressImpl();
	uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
	virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return timeoutUs; }
	virtual unsigned getRetryCount()                     const { return retryCnt;  }
	virtual INoSsiDev::ProtocolVersion getProtoVersion() const { return protoVersion; }
	virtual uint8_t  getVC()                             const { return vc; }
	virtual unsigned short getDport()                    const { return dport; }
	virtual uint32_t getTid()                            const { return ++tid; }
};

class NoSsiDevImpl : public DevImpl, public virtual INoSsiDev {
private:
	in_addr_t d_ip;
	string    ip_str;
public:
	NoSsiDevImpl(FKey key, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip; }

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 100, unsigned retryCnt = 5, uint8_t vc = 0);
};


#endif
