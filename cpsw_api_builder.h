#ifndef CPSW_API_BUILDER_H
#define CPSW_API_BUILDER_H

#include <cpsw_api_user.h>

// ************** BIG NOTE ****************
//   The builder API is NOT THREAD SAFE.
// ****************************************

using boost::static_pointer_cast;

class IVisitor;
class IField;
class CEntryImpl;
class FKey;
class IDev;

typedef shared_ptr<IField>     Field;
typedef shared_ptr<IDev>       Dev;
typedef shared_ptr<CEntryImpl> EntryImpl;

typedef enum ByteOrder { UNKNOWN           = 0, LE = 12, BE = 21 } ByteOrder;

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
public:
	virtual Cacheable getCacheable()                 const = 0;
	virtual void      setCacheable(Cacheable)              = 0;
	virtual void      setDescription(const char *)         = 0;
	virtual void      setDescription(const std::string &)  = 0;
	virtual ~IField() {}
	virtual EntryImpl getSelf()                            = 0;

	static Field create(const char *name, uint64_t size = 0);
};

class IDev : public virtual IHub, public virtual IField {
public:
	virtual  void addAtAddress( Field f, unsigned nelms = 1 ) = 0;
	virtual ~IDev() {}

	static Dev create(const char *name, uint64_t size = 0);

	static Dev getRootDev();
};

class IVisitor {
public:
	virtual void visit( Field ) = 0;
	virtual void visit( Dev d ) { visit( static_pointer_cast<Field::element_type, Dev::element_type>( d ) ); }
	virtual ~IVisitor() {}
};

class IMMIODev;

typedef shared_ptr<IMMIODev> MMIODev;

class IMMIODev : public virtual IDev {
public:
static const uint64_t STRIDE_AUTO = -1ULL;

	virtual void addAtAddress(Field child, uint64_t offset, unsigned nelms = 1, uint64_t stride = STRIDE_AUTO, ByteOrder byteOrder = UNKNOWN ) = 0;

	static MMIODev create(const char *name, uint64_t size, ByteOrder byteOrder = UNKNOWN);
};

class IMemDev;
typedef shared_ptr<IMemDev> MemDev;

class IMemDev : public virtual IDev {
public:
	virtual void            addAtAddress(Field child, unsigned nelms = 1) = 0;
	virtual uint8_t * const getBufp() const = 0;

	static MemDev create(const char *name, uint64_t size);
};


class INetIODev;
typedef shared_ptr<INetIODev> NetIODev;

class INetIODev : public virtual IDev {
public:
	typedef enum ProtocolVersion { SRP_UDP_NONE = -1, SRP_UDP_V1 = 1, SRP_UDP_V2 = 2, SRP_UDP_V3 = 3 } ProtocolVersion;

	class IPortBuilder;
	typedef shared_ptr<IPortBuilder> PortBuilder;

	class IPortBuilder {
	public:
		// Note: most of the parameters configured into a PortBuilder object are
		//       only used if the associated protocol module is not already present
		//       and they are ignored otherwise.
		//       E.g., if a UDP port is shared (via TDEST and or SRP VC multiplexers)
		//       and already present when adding a new TDEST/VC then the UDP parameters
		//       (queue depth, number of threads) are ignored.
		virtual void            setSRPVersion(ProtocolVersion)      = 0; // default: SRP_UDP_V2
		virtual ProtocolVersion getSRPVersion()                     = 0;
		virtual void            setSRPTimeoutUS(uint64_t)           = 0; // default: 10000 if no rssi, 500000 if rssi
		virtual uint64_t        getSRPTimeoutUS()                   = 0;
		virtual void            useSRPDynTimeout(bool)              = 0; // default: YES unless TDEST demuxer in use
		virtual bool            hasSRPDynTimeout()                  = 0; // dynamically adjusted timeout (based on RTT)
		virtual void            setSRPRetryCount(unsigned)          = 0; // default: 10
		virtual unsigned        getSRPRetryCount()                  = 0;
		virtual void            setSRPIgnoreMemResp(bool)           = 0;
		virtual bool            getSRPIgnoreMemResp()               = 0;

		virtual bool            hasUdp()                            = 0; // default: YES
		virtual void            setUdpPort(unsigned)                = 0; // default: 8192
		virtual unsigned        getUdpPort()                        = 0;
		virtual void            setUdpOutQueueDepth(unsigned)       = 0; // default: 10
		virtual unsigned        getUdpOutQueueDepth()               = 0;
		virtual void            setUdpNumRxThreads(unsigned)        = 0; // default: 1
		virtual unsigned        getUdpNumRxThreads()                = 0;
		virtual void            setUdpPollSecs(int)                 = 0; // default: NO if SRP w/o TDEST or RSSI, 60s if no SRP
		virtual int             getUdpPollSecs()                    = 0;

		virtual void            useRssi(bool)                       = 0; // default: NO
		virtual bool            hasRssi()                           = 0;

		virtual void            useDepack(bool)                     = 0; // default: NO
		virtual bool            hasDepack()                         = 0;
		virtual void            setDepackOutQueueDepth(unsigned)    = 0; // default: 50
		virtual unsigned        getDepackOutQueueDepth()            = 0;
		virtual void            setDepackLdFrameWinSize(unsigned)   = 0; // default: 5 if no rssi, 1 if rssi
		virtual unsigned        getDepackLdFrameWinSize()           = 0;
		virtual void            setDepackLdFragWinSize(unsigned)    = 0; // default: 5 if no rssi, 1 if rssi
		virtual unsigned        getDepackLdFragWinSize()            = 0;

		virtual void            useSRPMux(bool)                     = 0; // default: YES if SRP, NO if no SRP
		virtual bool            hasSRPMux()                         = 0;
		virtual void            setSRPMuxVirtualChannel(unsigned)   = 0; // default: 0
		virtual unsigned        getSRPMuxVirtualChannel()           = 0;

		virtual void            useTDestMux(bool)                   = 0; // default: NO
		virtual bool            hasTDestMux()                       = 0;
		virtual void            setTDestMuxTDEST(unsigned)          = 0; // default: 0
		virtual unsigned        getTDestMuxTDEST()                  = 0;
		virtual void            setTDestMuxStripHeader(bool)        = 0; // default: YES if SRP, NO if no SRP
		virtual bool            getTDestMuxStripHeader()            = 0;
		virtual void            setTDestMuxOutQueueDepth(unsigned)  = 0; // default: 1 if SRP, 50 if no SRP
		virtual unsigned        getTDestMuxOutQueueDepth()          = 0;

		virtual void            reset()                             = 0; // reset to defaults

		virtual PortBuilder     clone()                             = 0;
	};

	static PortBuilder createPortBuilder();

	virtual void addAtAddress(Field child, PortBuilder bldr)        = 0;

	// DEPRECATED -- use addAtAddress(Field, PortBuilder)
	virtual void addAtAddress(Field           child,
	                          ProtocolVersion version        = SRP_UDP_V2,
	                          unsigned        dport          =       8192,
	                          unsigned        timeoutUs      =       1000,
	                          unsigned        retryCnt       =         10,
	                          uint8_t         vc             =          0,
	                          bool            useRssi        =      false,
	                          int             tDest          =         -1
	) = 0;

	// DEPRECATED -- use addAtAddress(Field, PortBuilder)
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

	virtual const char *getIpAddressString() const = 0;

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
	typedef uint64_t (*TransformFunc)(uint64_t in);

	virtual void add(const char *enum_string, uint64_t enum_num) = 0;

	static MutableEnum create(TransformFunc f);

	// Create with variable argument list. The list are NULL-terminated
	// pairs of { const char *, int }:
	//
	//   create( f, "honey", 0, "marmelade", 2, NULL );
	//
	// NOTE: the numerical values are **int**. If you need 64-bit
	//       values then you must the 'add' method.
	static MutableEnum create(TransformFunc f, ...);

	static MutableEnum create();
};

extern Enum const enumBool;
extern Enum const enumYesNo;

class IIntField;
typedef shared_ptr<IIntField> IntField;

class IIntField: public virtual IField {
public:
	typedef enum Mode { RO = 1, WO = 2, RW = 3 } Mode;

	static const uint64_t DFLT_SIZE_BITS = 32;
	static const bool     DFLT_IS_SIGNED = false;
	static const int      DFLT_LS_BIT    =  0;
	static const Mode     DFLT_MODE      = RW;
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
	// Create a SequenceCommand
	//   This object takes a vector of entryNames and a vector of values
	//   When executed by the user it steps through and sets each entry to the
	//   associated value.
	//
	//   entryPath vector of strings of paths to IntField/SequenceCommand or "usleep"
	//   values is the associated value to put
	//
	//   Ex create a command that sleeps for 1 second and then puts 0xdeadbeef in val:
	//       std::vector<std::string> names;
	//       std::vector<uint64_t> values;
	//       names.push_back( "usleep" );
	//       values.push_back(1000000);
	//       names.push_back( "val" );
	//       values.push_back( (uint64_t)0xdeadbeef );
	//       ISequenceCommand::create("cmd", c_names, c_values);
	static SequenceCommand create(const char* name, std::vector<std::string> entryPath, std::vector<uint64_t> values);
}; 

#endif
