#ifndef CPSW_NOSSI_DEV_H
#define CPSW_NOSSI_DEV_H

#include <cpsw_hub.h>

// ugly - these shouldn't be here!
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

class NoSsiDev;

class UdpAddress : public Address {
protected:
	unsigned short dport;
	int            sd;
	unsigned       timeoutUs;
	unsigned       retryCnt;
	UdpAddress(NoSsiDev *owner, unsigned short dport, unsigned timeoutUs, unsigned retryCnt);
	friend class NoSsiDev;
public:
	virtual ~UdpAddress();
	virtual unsigned short getDport() const { return dport; }
	uint64_t read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;

	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()  const { return timeoutUs; }
	virtual unsigned getRetryCount() const { return retryCnt;  }
};

class NoSsiDev : public Dev {
private:
	in_addr_t d_ip;
public:
	NoSsiDev(const char *name, const char *ip);

	in_addr_t getIp() const { return d_ip; }

	virtual void addAtAddr(Entry *child, unsigned dport, unsigned timeoutUs=200, unsigned retryCnt=5)
	{
		UdpAddress *a = new UdpAddress(this, dport, timeoutUs, retryCnt);
		add(a, child);
	}

};


#endif
