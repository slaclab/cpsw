 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_API_BUILDER_H
#define CPSW_API_BUILDER_H

#include <cpsw_api_user.h>
#include <iostream>
#include <stdio.h>

/*!************** BIG NOTE ****************
 *   The builder API is NOT THREAD SAFE.
 ******************************************/

/*
 * The Builder API allows the user to programmatically
 * assemble a hierarchy of devices and endpoints without
 * having to know any implementation details.
 *
 * NOTE: The builder API has been superseeded by YAML.
 *       Application- and device-support designers are
 *       now encouraged to use a YAML file to define
 *       a hierarchy rather than coding C++.
 */

class IVisitor;
class IField;
class CEntryImpl;
class FKey;
class IDev;


typedef shared_ptr<IField>     Field;
typedef shared_ptr<IDev>       Dev;
typedef shared_ptr<CEntryImpl> EntryImpl;

namespace YAML {
	class PNode;
};

struct YamlState;

struct StrCmp {
	bool operator () (const char *a, const char *b ) const {
		return strcmp(a , b) < 0 ? true : false;
	}
};

typedef enum ByteOrder { UNKNOWN = 0, LE = 12, BE = 21, NATIVE = 22 } ByteOrder;

typedef enum WriteMode { POSTED = 0, SYNCHRONOUS = 1 } WriteMode;

ByteOrder hostByteOrder();

class IVisitable {
public:
	typedef enum RecursionOrder { RECURSE_DEPTH_FIRST = true, RECURSE_DEPTH_AFTER = false } RecursionOrder;
	static const int DEPTH_INDEFINITE = -1; // recurse into all leaves
	static const int DEPTH_NONE       =  0; // no recursion; current leaf only
	                                        // positive values give desired depth
	virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth = DEPTH_INDEFINITE) = 0;
	virtual ~IVisitable() {}
};

class IField : public virtual IEntry, public virtual IVisitable {
public:
	// the enum is ordered in increasing 'looseness' of the cacheable attribute
	typedef enum Cacheable { UNKNOWN_CACHEABLE = 0, NOT_CACHEABLE, WT_CACHEABLE, WB_CACHEABLE } Cacheable;

	static const uint64_t  DFLT_SIZE      = 0;
	static const Cacheable DFLT_CACHEABLE = UNKNOWN_CACHEABLE;

public:
	virtual Cacheable getCacheable()                 const = 0;
	virtual void      setCacheable(Cacheable)              = 0;
	virtual void      setDescription(const char *)         = 0;
	virtual void      setDescription(const std::string &)  = 0;
	virtual ~IField() {}
	virtual EntryImpl getSelf()                            = 0;

	static Field create(const char *name, uint64_t size = DFLT_SIZE);
};

class IDev : public virtual IHub, public virtual IField {
public:
	static const unsigned DFLT_NELMS = 1;
	virtual  void addAtAddress( Field f, unsigned nelms = DFLT_NELMS ) = 0;
	virtual ~IDev() {}

	static Dev create(const char *name, uint64_t size = DFLT_SIZE);

	// initialize so that IO can be done
	virtual void      startUp()                            = 0;
	virtual int       isStarted()                    const = 0;

	static Dev getRootDev();
};

class IVisitor {
public:
	virtual void visit( Field ) = 0;
	virtual void visit( Dev d );
	virtual ~IVisitor() {}
};

class IMMIODev;

typedef shared_ptr<IMMIODev> MMIODev;

class IMMIODev : public virtual IDev {
public:
static const uint64_t STRIDE_AUTO = -1ULL;

static const ByteOrder DFLT_BYTE_ORDER = UNKNOWN;
static const uint64_t  DFLT_STRIDE = STRIDE_AUTO;

	virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = DFLT_NELMS, uint64_t stride = DFLT_STRIDE, ByteOrder byteOrder = DFLT_BYTE_ORDER ) = 0;


	static MMIODev create(const char *name, uint64_t size, ByteOrder byteOrder = DFLT_BYTE_ORDER);
};

class IMemDev;
typedef shared_ptr<IMemDev> MemDev;

class IMemDev : public virtual IDev {
public:
	static const int DFLT_ALIGN = 1; /* in bytes; 1,2,4,8 */

	virtual void            addAtAddress(Field child, int align = DFLT_ALIGN) = 0;
	virtual uint8_t * const getBufp()                                   const = 0;

	static MemDev create(const char *name, uint64_t size, uint8_t *ext_buf = 0);
};

class INullDev;
typedef shared_ptr<INullDev> NullDev;

class INullDev : public virtual IDev {
public:
	virtual void            addAtAddress(Field child) = 0;

	static NullDev create(const char *name, uint64_t size);
};


class IProtoStackBuilder;
typedef shared_ptr<IProtoStackBuilder> ProtoStackBuilder;

class IProtoPort;
typedef shared_ptr<IProtoPort>         ProtoPort;

typedef const struct LibSocksProxy *LibSocksProxyPtr;

class IProtoStackBuilder {
public:
	typedef enum SRPProtoVersion    { SRP_UDP_NONE = -1, SRP_UDP_V1 = 1, SRP_UDP_V2 = 2, SRP_UDP_V3 = 3 } SRPProtoVersion;
	typedef enum DepackProtoVersion { DEPACKETIZER_V0 =  0, DEPACKETIZER_V2 = 2                         } DepackProtoVersion;

	const static int DFLT_THREAD_PRIORITY =  0;
	const static int NORT_THREAD_PRIORITY =  0;
	const static int   NO_THREAD_PRIORITY = -1; /* no thread in module */

	// Note: most of the parameters configured into a ProtoStackBuilder object are
	//       only used if the associated protocol module is not already present
	//       and they are ignored otherwise.
	//       E.g., if a UDP port is shared (via TDEST and or SRP VC multiplexers)
	//       and already present when adding a new TDEST/VC then the UDP parameters
	//       (queue depth, number of threads) are ignored.
	virtual void               setSRPVersion(SRPProtoVersion)      = 0; // default: SRP_UDP_V2
	virtual SRPProtoVersion    getSRPVersion()                     = 0;
	virtual void               setSRPTimeoutUS(uint64_t)           = 0; // default: 10000 if no rssi, 500000 if rssi
	virtual uint64_t           getSRPTimeoutUS()                   = 0;
	virtual void               useSRPDynTimeout(bool)              = 0; // default: YES unless TDEST demuxer in use
	virtual bool               hasSRPDynTimeout()                  = 0; // dynamically adjusted timeout (based on RTT)
	virtual void               setSRPRetryCount(unsigned)          = 0; // default: 10
	virtual unsigned           getSRPRetryCount()                  = 0;

	virtual void               setSRPDefaultWriteMode(WriteMode)   = 0; // default: POSTED
	virtual WriteMode          getSRPDefaultWriteMode()            = 0;

	virtual bool               hasTcp()                            = 0; // default: NO
    virtual void               setTcpPort(unsigned)                = 0; // default: 8192
	virtual unsigned           getTcpPort()                        = 0;
	virtual void               setTcpOutQueueDepth(unsigned)       = 0; // default: 50 (20 with SRP)
	virtual unsigned           getTcpOutQueueDepth()               = 0;
	virtual void               setTcpThreadPriority(int)           = 0;
	virtual int                getTcpThreadPriority()              = 0;

	virtual bool               hasUdp()                            = 0; // default: YES
	virtual void               setUdpPort(unsigned)                = 0; // default: 8192
	virtual unsigned           getUdpPort()                        = 0;
	virtual void               setUdpOutQueueDepth(unsigned)       = 0; // default: 10
	virtual unsigned           getUdpOutQueueDepth()               = 0;
	virtual void               setUdpNumRxThreads(unsigned)        = 0; // default: 1
	virtual unsigned           getUdpNumRxThreads()                = 0;
	virtual void               setUdpPollSecs(int)                 = 0; // default: NO if SRP w/o TDEST or RSSI, 60s if no SRP
	virtual int                getUdpPollSecs()                    = 0;
	virtual void               setUdpThreadPriority(int)           = 0;
	virtual int                getUdpThreadPriority()              = 0;

	virtual void               useRssi(bool)                       = 0; // default: NO
	virtual bool               hasRssi()                           = 0;
	virtual bool               hasRssiAndUdp()                     = 0;
	virtual void               setRssiThreadPriority(int)          = 0;
	virtual int                getRssiThreadPriority()             = 0;

	virtual void               useDepack(bool)                     = 0; // default: NO
	virtual void               setDepackVersion(DepackProtoVersion)= 0;
	virtual DepackProtoVersion getDepackVersion()                  = 0;
	virtual bool               hasDepack()                         = 0;
	virtual void               setDepackOutQueueDepth(unsigned)    = 0; // default: 50
	virtual unsigned           getDepackOutQueueDepth()            = 0;
	virtual void               setDepackLdFrameWinSize(unsigned)   = 0; // default: 5 if no rssi, 1 if rssi
	virtual unsigned           getDepackLdFrameWinSize()           = 0;
	virtual void               setDepackLdFragWinSize(unsigned)    = 0; // default: 5 if no rssi, 1 if rssi
	virtual unsigned           getDepackLdFragWinSize()            = 0;
	virtual void               setDepackThreadPriority(int)        = 0;
	virtual int                getDepackThreadPriority()           = 0;

	virtual void               useSRPMux(bool)                     = 0; // default: YES if SRP, NO if no SRP
	virtual bool               hasSRPMux()                         = 0;
	virtual void               setSRPMuxVirtualChannel(unsigned)   = 0; // default: 0
	virtual unsigned           getSRPMuxVirtualChannel()           = 0;
	virtual void               setSRPMuxOutQueueDepth(unsigned)    = 0; // default: 50 (async queue)
	virtual unsigned           getSRPMuxOutQueueDepth()            = 0;
	virtual void               setSRPMuxThreadPriority(int)        = 0;
	virtual int                getSRPMuxThreadPriority()           = 0;

	virtual void               useTDestMux(bool)                   = 0; // default: NO
	virtual bool               hasTDestMux()                       = 0;
	virtual void               setTDestMuxTDEST(unsigned)          = 0; // default: 0
	virtual unsigned           getTDestMuxTDEST()                  = 0;
	virtual void               setTDestMuxStripHeader(bool)        = 0; // default: YES if SRP, NO if no SRP
	virtual bool               getTDestMuxStripHeader()            = 0;
	virtual void               setTDestMuxOutQueueDepth(unsigned)  = 0; // default: 50
	virtual unsigned           getTDestMuxOutQueueDepth()          = 0;
	virtual void               setTDestMuxInpQueueDepth(unsigned)  = 0; // default: 50; only applicable for TDestMux2
	virtual unsigned           getTDestMuxInpQueueDepth()          = 0;
	virtual void               setTDestMuxThreadPriority(int)      = 0;
	virtual int                getTDestMuxThreadPriority()         = 0;

	virtual void               setIPAddr(uint32_t)                 = 0;
	virtual uint32_t           getIPAddr()                         = 0;

	virtual void               setRssiBridgeIPAddr(uint32_t)       = 0;
	virtual uint32_t           getRssiBridgeIPAddr()               = 0;
	virtual bool               hasRssiBridge()                     = 0;

	virtual void               setSocksProxy(LibSocksProxyPtr)     = 0;
	virtual LibSocksProxyPtr   getSocksProxy()                     = 0;

	virtual void               reset()                             = 0; // reset to defaults

	virtual bool               getAutoStart()                      = 0;
	virtual void               setAutoStart(bool)                  = 0;

	virtual ProtoPort          build(std::vector<ProtoPort>&)      = 0;

	virtual ProtoStackBuilder    clone()                           = 0;

	static ProtoStackBuilder create();
	static ProtoStackBuilder create(YamlState &);
};


class INetIODev;
typedef shared_ptr<INetIODev> NetIODev;

class INetIODev : public virtual IDev {
public:
	// DEPRECATED
	typedef IProtoStackBuilder::SRPProtoVersion ProtocolVersion;


	virtual void addAtAddress(Field child, ProtoStackBuilder bldr)        = 0;

#if 0
	// DEPRECATED -- use addAtAddress(Field, ProtoStackBuilder)
	virtual void addAtAddress(Field           child,
	                          ProtocolVersion version        = SRP_UDP_V2,
	                          unsigned        dport          =       8192,
	                          unsigned        timeoutUs      =       1000,
	                          unsigned        retryCnt       =         10,
	                          uint8_t         vc             =          0,
	                          bool            useRssi        =      false,
	                          int             tDest          =         -1
	) = 0;

	// DEPRECATED -- use addAtAddress(Field, ProtoStackBuilder)
	virtual void addAtStream(Field            child,
	                         unsigned         dport,
	                         unsigned         timeoutUs,
	                         unsigned         inQDepth       =         32,
	                         unsigned         outQDepth      =         16,
	                         unsigned         ldFrameWinSize =          4,
	                         unsigned         ldFragWinSize  =          4,
	                         unsigned         nUdpThreads    =          2,
	                         bool             useRssi        =      false,
	                         int              tDest          =         -1
	) = 0;
#endif

	virtual const char *getIpAddressString()  const = 0;
	virtual const char *getSocksProxyString() const = 0;
	virtual const char *getRssiBridgeString() const = 0;

	static NetIODev create(const char *name, const char *ipaddr);
};

class IMutableEnum;
typedef shared_ptr<IMutableEnum> MutableEnum;

class IMutableEnum : public IEnum {
public:
	// optional function to transform a numerical
	// value before applying the reverse map:
	//
	//  read -> transform -> mapFromNumToString
	//
	// This can be used, e.g., to map any nonzero
	// value to 1 for a boolean-style enum.

	class CTransformFunc;
	typedef CTransformFunc *TransformFunc;

	class CTransformFunc {
	private:
		CTransformFunc(const CTransformFunc &);
		CTransformFunc operator=(const CTransformFunc&);
		const char *name_;
	protected:
		class Key {
		private:
			const char *tag_;
			Key(const Key &);
			Key operator=(const Key&);
			Key(const char *tag) :tag_(tag){}
		friend class CTransformFunc;
		};
	public:
		// can only instantiate once
		CTransformFunc(const Key &key);

		virtual uint64_t xfrm(uint64_t in)
		{
			return in;
		}

		virtual const char *getName()
		{
			return name_;
		}

		virtual ~CTransformFunc();

		template <typename T> static TransformFunc get()
		{
		static T the_instance_( Key( T::getName_() ) );
			return &the_instance_;
		}
	};

	virtual void add(const char *enum_string, uint64_t enum_num) = 0;

	static MutableEnum create(TransformFunc);

	// Create with variable argument list. The list are NULL-terminated
	// pairs of { const char *, int }:
	//
	//   create( f, "honey", 0, "marmelade", 2, NULL );
	//
	// NOTE: the numerical values are **int**. If you need 64-bit
	//       values then you must the 'add' method.
	static MutableEnum create(TransformFunc, ...);

	static MutableEnum create();

	static MutableEnum create(YamlState &node);
};

extern Enum const enumBool;
extern Enum const enumYesNo;

class IIntField;
typedef shared_ptr<IIntField> IntField;

class IIntField: public virtual IField {
public:
	// Introduce IIntField::Mode for BWDs compatibiliy
    typedef IVal_Base::Mode Mode;
	static const enum IVal_Base::Mode RW = IVal_Base::RW;
	static const enum IVal_Base::Mode RO = IVal_Base::RO;
	static const enum IVal_Base::Mode WO = IVal_Base::WO;

	static const uint64_t DFLT_SIZE_BITS = 32;
	static const bool     DFLT_IS_SIGNED = false;
	static const int      DFLT_LS_BIT    =  0;
	static const Mode     DFLT_MODE      = IVal_Base::RW;
	static const unsigned DFLT_WORD_SWAP =  0;

	class IBuilder;
	typedef shared_ptr<IBuilder> Builder;

	class IBuilder {
	public:
		virtual Builder name(const char *)    = 0;
		virtual Builder sizeBits(uint64_t)    = 0;
		virtual Builder isSigned(bool)        = 0;
		virtual Builder lsBit(int)            = 0;
		virtual Builder mode(Mode)            = 0;
		virtual Builder wordSwap(unsigned)    = 0;
		virtual Builder setEnum(Enum)         = 0;

		virtual Builder reset()               = 0;

		virtual IntField build()              = 0;
		virtual IntField build(const char*)   = 0;

		virtual Builder clone() = 0;

		static Builder create();
	};

	virtual bool     isSigned()    const = 0;
	virtual int      getLsBit()    const = 0;
	virtual uint64_t getSizeBits() const = 0;
	virtual Mode     getMode()     const = 0;

	static IntField create(const char *name, uint64_t sizeBits = DFLT_SIZE_BITS, bool is_Signed = DFLT_IS_SIGNED, int lsBit = DFLT_LS_BIT, Mode mode = DFLT_MODE, unsigned wordSwap = DFLT_WORD_SWAP);
};

class ISequenceCommand;
typedef shared_ptr<ISequenceCommand> SequenceCommand;

class ISequenceCommand: public virtual IField {
public:

	typedef std::pair<std::string, uint64_t> Item;
	typedef std::vector<Item>                Items;

	// Create a SequenceCommand
	//   This object takes a vector of entryNames and a vector of values
	//   When executed by the user it steps through and sets each entry to the
	//   associated value.
	//
	//   entryPath vector of strings of paths to IntField/SequenceCommand or "usleep"
	//   values is the associated value to put
	//
	//   Ex create a command that sleeps for 1 second and then puts 0xdeadbeef in val:
	//       std::vector< ISequenceCommand::Item > items;
	//       items.push_back( ISequenceCommand::Item("usleep", 1000000) );
	//       items.push_back( ISequenceCommand::Item("val"   , (uint64_t)0xdeadbeef );
	//       ISequenceCommand::create("cmd", &items);

	static SequenceCommand create(const char* name, const Items *items_p);
};

// dump existing hierarchy to YAML file (for testing & debugging)
namespace IYamlSupport {
	// if 'root_entry_name' is specified then the top node will receive this
	// as a key:
	//
	// 'root_entry_name':
	//     top
	//
	void dumpYamlFile(Entry top, const char *filename, const char *root_entry_name);

	// Load YAML but do not start I/O
	Dev  buildHierarchy(const char *yaml_name,
	                    const char *root_name  = "root", // compatibility with IPath::loadYamlFile
	                    const char *yaml_dir   = 0,
	                    IYamlFixup *fixup      = 0);

	Dev  buildHierarchy(std::istream &in,
	                    const char *root_name  = 0,      // compatibility with IPath::loadYamlFile
	                    const char *yaml_dir   = 0,
	                    IYamlFixup *fixup      = 0);

	Path startHierarchy(Dev);

	// return the number of unrecognized/unsupported YAML keys
	// found when the (last) hierarchy was built.
	unsigned long getNumberOfUnrecognizedKeys();
};
#endif
