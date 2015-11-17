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
protected:
	unsigned short dport;
	int            sd;
	unsigned       timeoutUs;
	unsigned       retryCnt;
public:
	CUdpAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned retryCnt);
	virtual ~CUdpAddressImpl();
	virtual unsigned short getDport() const { return dport; }
	uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
	virtual uint64_t write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()  const { return timeoutUs; }
	virtual unsigned getRetryCount() const { return retryCnt;  }
};

class CNoSsiDevImpl : public CDevImpl, public virtual INoSsiDev {
private:
	in_addr_t d_ip;
	string    ip_str;
public:
	CNoSsiDevImpl(FKey key, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip; }

	virtual void addAtAddress(Field child, unsigned dport, unsigned timeoutUs, unsigned retryCnt);
};


#endif
