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
	int      sd;
	UdpAddress(NoSsiDev *owner, unsigned short dport);
	friend class NoSsiDev;
public:
	virtual ~UdpAddress();
	virtual unsigned short getDport() const { return dport; }
	uint64_t read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const;
};

class NoSsiDev : public Dev {
private:
	in_addr_t d_ip;
public:
	NoSsiDev(const char *name, const char *ip);

	in_addr_t getIp() const { return d_ip; }

	virtual void addAtAddr(Entry *child, unsigned dport)
	{
		UdpAddress *a = new UdpAddress(this, dport);
		add(a, child);
	}
};


#endif
