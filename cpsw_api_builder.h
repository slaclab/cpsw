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


class INoSsiDev;
typedef shared_ptr<INoSsiDev> NoSsiDev;

class INoSsiDev : public virtual IDev {
public:
	typedef enum ProtocolVersion { SRP_UDP_V1 = 1, SRP_UDP_V2 = 2 } ProtocolVersion;

	virtual void addAtAddress(Field child, ProtocolVersion version = SRP_UDP_V2, unsigned dport = 8192, unsigned timeoutUs = 1000, unsigned retryCnt = 5, uint8_t vc = 0) = 0;
	virtual void addAtStream(Field child, unsigned dport, unsigned timeoutUs, unsigned inQDepth = 32, unsigned outQDepth = 16, unsigned ldFrameWinSize = 4, unsigned ldFragWinSize = 4, unsigned nUdpThreads = 2) = 0;

	virtual const char *getIpAddressString() const = 0;

	static NoSsiDev create(const char *name, const char *ipaddr);
};

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

#endif
