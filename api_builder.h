#ifndef CPSW_API_BUILDER_H
#define CPSW_API_BUILDER_H

#include <api_user.h>
#include <boost/interprocess/smart_ptr/unique_ptr.hpp>

using boost::interprocess::unique_ptr;
using boost::static_pointer_cast;

class IVisitor;
class IField;
class IAddress;
class IDev;

typedef shared_ptr<IAddress> Address;
typedef shared_ptr<IField>   Field;
typedef shared_ptr<IDev>     Dev;

class IVisitable {
public:
	typedef enum RecursionOrder { RECURSE_DEPTH_FIRST = true, RECURSE_DEPTH_AFTER = false } RecursionOrder;
	static const int DEPTH_INDEFINITE = -1; // recurse into all leaves
	static const int DEPTH_NONE       =  0; // no recursion; current leaf only
                                            // positive values give desired depth
	virtual void accept(IVisitor *v, RecursionOrder order, int recursionDepth = DEPTH_INDEFINITE) = 0;
	virtual ~IVisitable() {}
};

class EntryImpl;
typedef shared_ptr<EntryImpl> Entry;

typedef enum ByteOrder { UNKNOWN           = 0, LE = 12, BE = 21 } ByteOrder;

ByteOrder hostByteOrder();


class IField : public virtual IEntry, public virtual IVisitable {
public:
	typedef enum Cacheable { UNKNOWN_CACHEABLE = 0, NOT_CACHEABLE, WT_CACHEABLE, WB_CACHEABLE } Cacheable;
public:
	virtual Cacheable getCacheable()           const = 0;
	virtual void      setCacheable(Cacheable)        = 0;
	virtual ~IField() {}
	virtual Entry     getSelf()                      = 0;

	static Field create(const char *name, uint64_t size = 0);
};

class IDev : public virtual IHub, public virtual IField {
public:
	virtual  void addAtAddress( Field f, unsigned nelms = 1 ) = 0;
	virtual ~IDev() {}

	static Dev create(const char *name, uint64_t size = 0);
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
	virtual uint8_t * const getBufp() = 0;

	static MemDev create(const char *name, uint64_t size);
};


class INoSsiDev;
typedef shared_ptr<INoSsiDev> NoSsiDev;

class INoSsiDev : public virtual IDev {
public:
	virtual void addAtAddress(Field child, unsigned dport, unsigned timeoutUs = 200, unsigned retryCnt = 5) = 0;
	virtual const char *getIpAddressString() const = 0;

	static NoSsiDev create(const char *name, const char *ipaddr);
};

class IIntField;
typedef shared_ptr<IIntField> IntField;

class IIntField: public virtual IField {
public:
	virtual bool     isSigned()    const = 0;
	virtual int      getLsBit()    const = 0;
	virtual uint64_t getSizeBits() const = 0;

	static IntField create(const char *name, uint64_t sizeBits, bool is_Signed = false, int lsBit = 0, unsigned wordSwap=0);
};
#endif
