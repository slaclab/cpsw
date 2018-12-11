 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_SRP_ADDRESS_H
#define CPSW_SRP_ADDRESS_H

#include <cpsw_comm_addr.h>
#include <cpsw_thread.h>
#include <cpsw_async_io.h>


// Dynamical timeout based on round-trip times
class DynTimeout {
private:
	static const uint64_t AVG_SHFT  = 6; // time-constant for averager v(n+1) = v(n) + (x - v(n))/(1<<AVG_SHFT)
	static const uint64_t MARG_SHFT = 2; // timeout is avg << MARG_SHFT
	static const uint64_t CAP_US    = 5000; // empirical low-limit (under non-RT system)
	CTimeout  maxRndTrip_;
	uint64_t  avgRndTrip_;
	CTimeout  lastUpdate_;
	CTimeout  dynTimeout_;
	unsigned  nSinceLast_;
	uint64_t  timeoutCap_;
protected:
	void  setLastUpdate();
public:
	DynTimeout(const CTimeout &iniv);

	const CTimeout &get()  const { return  dynTimeout_; }
	const CTimeout *getp() const { return &dynTimeout_; }

	const CTimeout &getMaxRndTrip() const { return maxRndTrip_; }
	const CTimeout  getAvgRndTrip() const;

	void update(const struct timespec *now, const struct timespec *then);

	void setTimeoutCap(uint64_t cap)
	{
		timeoutCap_ = cap;
		if ( dynTimeout_.getUs() < cap )
			dynTimeout_.set( cap );
	}

	void relax();
	void reset(const CTimeout &);

};

class CSRPAddressImpl;

class CSRPAsyncHandler : CRunnable {
private:
	AsyncIOTransactionManager xactMgr_;	
	CSRPAddressImpl          *srp_;
	ProtoDoor                 door_;
	volatile unsigned         stats_;
	virtual bool threadStop(void **);
protected:
	virtual void *threadBody();
public:
	CSRPAsyncHandler(AsyncIOTransactionManager, CSRPAddressImpl *);
	virtual void threadStart(ProtoDoor);
	virtual bool threadStop()
	{
		return CRunnable::threadStop();
	}

	virtual ProtoDoor getDoor() const
	{
		return door_;
	}

	virtual unsigned getMsgCount() const
	{
		return stats_;
	}
};

class CSRPAddressImpl : public CCommAddressImpl {
private:
	INetIODev::ProtocolVersion protoVersion_;
	CTimeout                  usrTimeout_;
	mutable DynTimeout        dynTimeout_;
	bool                      useDynTimeout_;
	unsigned                  retryCnt_;
	mutable unsigned          nRetries_;
	mutable unsigned          nWrites_;
	mutable unsigned          nReads_;
	uint8_t                   vc_;
	bool                      needsSwap_;
	bool                      needsPldSwap_;
	mutable uint32_t          tid_;
	uint32_t                  tidMsk_;
	uint32_t                  tidLsb_;
	bool                      byteResolution_;
	unsigned                  maxWordsRx_;
	unsigned                  maxWordsTx_;
	WriteMode                 defaultWriteMode_;
	ProtoPort                 asyncIOPort_;
	AsyncIOTransactionManager asyncXactMgr_;
	CSRPAsyncHandler          asyncIOHandler_;

	BufChain         assembleXBuf(struct srp_iovec *iov, unsigned iovlen, int iov_pld, int toput) const;

protected:
	mutable CMtx     mutex_;
	virtual uint64_t readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes, AsyncIO aio) const;
	virtual uint64_t readBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, uint64_t off, unsigned sbytes) const;
	virtual uint64_t writeBlk_unlocked(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const;

public:
	CSRPAddressImpl(AKey key, ProtoStackBuilder, ProtoPort);

	CSRPAddressImpl(CSRPAddressImpl &orig, AKey k)
	: CCommAddressImpl(orig, k),
	  dynTimeout_(orig.dynTimeout_.get()),
	  nRetries_(0),
	  asyncIOHandler_( AsyncIOTransactionManager(), 0 )
	{
		throw InternalError("Clone not implemented"); /* need to clone mutex, ... */
	}

	virtual ~CSRPAddressImpl();
	virtual int      open (CompositePathIterator *node);
	virtual int      close(CompositePathIterator *node);
	virtual uint64_t read(CompositePathIterator *node,  CReadArgs *args)  const;
	virtual uint64_t write(CompositePathIterator *node, CWriteArgs *args) const;

	virtual void dump(FILE *f) const;

	virtual void startProtoStack();

	// ANY subclass must implement clone(AKey) !
	virtual CSRPAddressImpl *clone(AKey k) { return new CSRPAddressImpl( *this, k ); }
	virtual void     setTimeoutUs(unsigned timeoutUs);
	virtual void     setRetryCount(unsigned retryCnt);
	virtual unsigned getTimeoutUs()                      const { return usrTimeout_.getUs();               }
	virtual unsigned getDynTimeoutUs()                   const { return dynTimeout_.get().getUs();         }
	virtual unsigned getRetryCount()                     const { return retryCnt_;                         }
	virtual INetIODev::ProtocolVersion getProtoVersion() const { return protoVersion_;                     }
	virtual bool     getByteResolution()                 const { return byteResolution_;                   }
	virtual uint8_t  getVC()                             const { return vc_;                               }
	virtual uint32_t toTid(uint32_t bits)                const { return bits & tidMsk_;                    }
	virtual uint32_t getTid()                            const { return toTid( tid_ = (tid_ + tidLsb_) );  }
	virtual bool     tidMatch(uint32_t a, uint32_t b)    const { return ! ((a ^ b) & tidMsk_);             }
	virtual bool     needsHdrSwap()                      const { return needsSwap_;                        }
	virtual bool     needsPayloadSwap()                  const { return needsPldSwap_;                     }
	virtual void     dumpYamlPart(YAML::Node &) const;
	virtual uint32_t extractTid(BufChain msg) const;
};

#endif
