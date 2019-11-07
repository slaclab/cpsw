 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_NETIO_DEV_H
#define CPSW_NETIO_DEV_H

#include <cpsw_hub.h>
#include <socks/libSocks.h>

#include <vector>
#include <string>

class CNetIODevImpl;
typedef shared_ptr<CNetIODevImpl> NetIODevImpl;

class CNetIODevImpl : public CDevImpl, public virtual INetIODev {
private:
	uint32_t         d_ip_;
	std::string      ip_str_;
	LibSocksProxy    socks_proxy_;
	std::string      socks_proxy_str_;
	uint32_t         rssi_bridge_ip_;
	std::string      rssi_bridge_str_;
protected:
	CNetIODevImpl(const CNetIODevImpl &orig, Key &k)
	: CDevImpl        ( orig, k               ),
	  d_ip_           ( orig.d_ip_            ),
	  ip_str_         ( orig.ip_str_          ),
	  socks_proxy_    ( orig.socks_proxy_     ),
	  socks_proxy_str_( orig.socks_proxy_str_ ),
	  rssi_bridge_ip_ ( orig.rssi_bridge_ip_  ),
	  rssi_bridge_str_( orig.rssi_bridge_str_ )
	{
		/* The real work is in cloning the protocols -  which is not supported */
	}

	virtual void getPorts( std::vector<ProtoPort> * );

public:
	CNetIODevImpl(Key &key, const char *name, const char *ip);

	CNetIODevImpl(Key &k, YamlState &n);
	virtual void dumpYamlPart(YAML::Node &) const;

	virtual void addAtAddress(Field child, YamlState &n);

	static  const char *_getClassName()       { return "NetIODev";      }
	virtual const char * getClassName() const { return _getClassName(); }

	virtual const char *getIpAddressString()     const { return ip_str_.c_str(); }
	virtual uint32_t    getIpAddress()           const { return d_ip_; }

	virtual const char *getSocksProxyString()    const { return socks_proxy_str_.c_str(); }
	virtual const char *getRssiBridgeString()    const { return rssi_bridge_str_.c_str(); }

	virtual uint32_t    getRssiBridgeIp()        const { return rssi_bridge_ip_; }

	virtual const LibSocksProxy &getSocksProxy() const { return socks_proxy_; }

	virtual CNetIODevImpl *clone(Key &k) { return new CNetIODevImpl( *this, k ); }

	virtual void dump(FILE *f) const;

	virtual void addAtAddress(Field child, ProtoStackBuilder bldr);

	virtual void startUp();

	virtual void addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs = 1000, unsigned retryCnt = 5, uint8_t vc = 0, bool useRssi = false, int tDest = -1);
	virtual void addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth, unsigned outQDepth, unsigned ldFrameWinSize, unsigned ldFragWinSize, unsigned nUdpThreads, bool useRssi = false, int tDest = -1);

	virtual void setLocked();
};

#endif
