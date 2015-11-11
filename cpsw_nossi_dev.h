#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

class NoSsiDevImpl;

class UdpAddressImpl : public AddressImpl {
protected:
	unsigned short dport;
	int            sd;
	unsigned       timeoutUs;
	unsigned       retryCnt;
public:
	UdpAddressImpl(AKey key, unsigned short dport, unsigned timeoutUs, unsigned retryCnt);
	virtual ~UdpAddressImpl();
	virtual unsigned short getDport() const { return dport; }
	uint64_t read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;

	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()  const { return timeoutUs; }
	virtual unsigned getRetryCount() const { return retryCnt;  }
};

class NoSsiDevImpl : public DevImpl, public virtual INoSsiDev {
private:
	in_addr_t d_ip;
	string    ip_str;
public:
	NoSsiDevImpl(FKey key, const char *ip);

	virtual const char *getIpAddressString() const { return ip_str.c_str(); }
	virtual in_addr_t   getIpAddress()       const { return d_ip; }

	virtual void addAtAddress(Field child, unsigned dport, unsigned timeoutUs, unsigned retryCnt);
};


#endif
