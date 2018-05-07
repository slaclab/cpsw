//@C Copyright Notice
//@C ================
//@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
//@C file found in the top-level directory of this distribution and at
//@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
//@C
//@C No part of CPSW, including this file, may be copied, modified, propagated, or
//@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_SRP_TRANSACTIONS_H
#define CPSW_SRP_TRANSACTIONS_H

#include <stdint.h>
#include <cpsw_srp_addr.h>
#include <cpsw_async_io.h>
#include <boost/shared_ptr.hpp>

using boost::shared_ptr;

class CSRPAddressImpl;

typedef uint32_t SRPWord;

typedef struct srp_iovec {
	void  *iov_base;
	size_t iov_len;
} IOVec;

class CSRPTransaction {
private:
	uint32_t  tid_;
	bool      doSwapV1_; // V1 sends payload in network byte order. We want to transform to restore
	                     // the standard AXI layout which is little-endian

	bool      doSwap_;   // header info needs to be swapped to host byte order since we interpret it

public:
	CSRPTransaction(
		const CSRPAddressImpl *srpAddr
	);

	void
	reset(const CSRPAddressImpl *srpAddr);

	virtual uint32_t getTid() const
	{
		return tid_;
	}

	virtual void complete(BufChain) = 0;

	virtual ~CSRPTransaction()
	{
	}

	virtual bool
	needsHdrSwap() const
	{
		return doSwap_;
	}

	virtual bool
	needsPayloadSwap() const
	{
		return doSwapV1_;
	}
};

class CSRPWriteTransaction : public CSRPTransaction {
private:
	int        nWords_;
	int        expected_;

protected:
	void
	resetMe(const CSRPAddressImpl *srpAddr, int nWords, int expected);

public:
	CSRPWriteTransaction(
		const CSRPAddressImpl *srpAddr,
		int   nWords,
		int   expected
	);

	void
	reset(const CSRPAddressImpl *srpAddr, int nWords, int expected);

	virtual void complete(BufChain);

};

class CSRPAsyncReadTransaction;
typedef shared_ptr<CSRPAsyncReadTransaction> SRPAsyncReadTransaction;

class CSRPReadTransaction : public CSRPTransaction {
private:
	uint8_t        *dst_;
    uint64_t        off_;
    unsigned        sbytes_;
    unsigned        headBytes_;
	int             xbufWords_;
	int             expected_;
	unsigned        hdrWords_;
	SRPWord         xbuf_[5];


    SRPWord         v1Header_;
    SRPWord         srpStatus_;
	IOVec           iov_[5];
	unsigned        iovLen_;
	SRPWord         rHdrBuf_[5];
	uint8_t         rTailBuf_[sizeof(SRPWord)];
	unsigned        tailBytes_;

protected:
	void
	resetMe(const CSRPAddressImpl *srpAddr, uint8_t *dst, uint64_t off, unsigned sbytes);

public:
	CSRPReadTransaction(
		const CSRPAddressImpl *srpAddr,
		uint8_t               *dst,
		uint64_t               off,
		unsigned               sbytes
	);

	void
	reset(const CSRPAddressImpl *srpAddr, uint8_t *dst, uint64_t off, unsigned sbytes);

	int
	getTotbytes() const
	{
		return headBytes_ + sbytes_;
	}

	int
	getNWords()   const
	{
		return (getTotbytes() + sizeof(SRPWord) - 1)/sizeof(SRPWord);
	}

    virtual void
	post(ProtoDoor, unsigned);

    virtual void
	complete(BufChain rchn);
};

class CSRPAsyncReadTransaction : public CAsyncIOTransaction, public CSRPReadTransaction{
public:
	CSRPAsyncReadTransaction(
		const CAsyncIOTransactionKey &key);

	virtual void complete(BufChain bc)
	{
		// if bc is NULL then a timeout occurred and we don't do anything
		if ( bc )
			CSRPReadTransaction::complete(bc);
	}

	virtual AsyncIOTransaction getSelfAsAsyncIOTransaction()
	{
		return getSelfAs<AsyncIOTransaction>();
	}
};

#endif
