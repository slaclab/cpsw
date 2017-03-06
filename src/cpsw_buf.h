 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_BUF_H
#define CPSW_BUF_H

#include <boost/shared_ptr.hpp>
#include <stdint.h>
#include <time.h>

#include <cpsw_event.h>

using boost::shared_ptr;

class IBuf;
class IBufChain;
class IBufQueue;

typedef shared_ptr<IBuf> Buf;
typedef shared_ptr<IBufChain> BufChain;
typedef shared_ptr<IBufQueue> BufQueue;

// NOTE: Buffer chains are NOT THREAD SAFE. It is the user's responsibility
//       to properly synchronize.

// NOTE: In order to avoid circular references of smart pointers
//       the backwards link ('Prev') of a doubly-linked chain
//       is 'weak', i.e., a chain is held together by the 'Next'
//       pointers.
//       Thus, it is not possible to have a linked list with only
//       a tail pointer (since all the list elements would be destroyed
//       -- unless there are other references).

//   +++> strong reference
//   ---> weak   reference
//
//              HEAD                 TAIL
//               +                    +
//               +                    +
//               V                    V
//              NODE:      NODE:     NODE:
//              next_ ++>  next_ ++> next_ ++> (NULL)
//       (NULL) prev_ <--  prev_ <-- prev_
//
// The 'forward' chain of strong references starting at 'HEAD' keeps
// the chain 'alive'. Without HEAD only the last node would 'survive'
// since there would be no strong references to other nodes.


class IBuf {
public:
	// max capacity
	virtual size_t   getCapacity()        = 0;
	// start of payload
	virtual uint8_t *getPayload()         = 0;
	// used portion (payload_end - payload_start + 1)
	virtual size_t   getSize()            = 0;
	// available portion at tail (capacity - payload_end)
	virtual size_t   getAvail()           = 0;
	// available portion at head (payload_start - buffer_start)
	virtual size_t   getHeadroom()        = 0;
	//***************************************************************
	// I.e., getHeadroom() + getSize() + getAvail() == getCapacity()
	//***************************************************************

	virtual void     setSize(size_t)      = 0;
	// setting payload ptr to 0 sets to start
	// of buffer
	virtual void     setPayload(uint8_t*) = 0;
	// move payload pointer (adjusting the size)
	// Returns 'true' on success and 'false' if
	// there would not be space (payload + size
	// left unmodified)
	virtual bool     adjPayload(ssize_t)  = 0;
	// reset payload ptr to start and size to capacity
	virtual void     reinit()             = 0;


	virtual Buf      getNext()            = 0;
	virtual Buf      getPrev()            = 0;

	virtual BufChain getChain()           = 0;

	// NOTE: none of the link manipulations
	//       are thread-safe
	// link this after 'buf'
	virtual void     after(Buf buf)       = 0;
	// link this before 'buf'
	virtual void     before(Buf buf)      = 0;
	// unlink (remove this buffer from a list)
	virtual void     unlink()             = 0;
	// split (result are two lists with this buffer
	// at the head of the second one)
	virtual void     split()              = 0;

	virtual         ~IBuf() {}

	const static size_t CAPA_ETH_BIG = 1500 - 20 - 8;
	const static size_t CAPA_ETH_JUM = 9000;
	const static size_t CAPA_ETH_HDR = 128;
	const static size_t CAPA_MAX     = 65535; // only 64k afaik - but this is enough

	// 'clip' == false throws if  request for 'capa' cannot
	// be satisfied. 'clip' == true clips to max. available
	// (caller can check via 'getCapacity()').
	static Buf       getBuf(size_t capa, bool clip = false);

	// if no buffers are available on the free-list then
	// they are allocated from the heap. After use they go
	// to the free-list.
	static unsigned  numBufsAlloced(); // from heap
	static unsigned  numBufsFree();    // on free-list
	static unsigned  numBufsInUse();
};


class IBufChain {
public:
	// The 'ownership' members are needed because lockfree::queue
	// cannot hold smart pointers. Therefore, before pushing a
	// smart pointer onto a lockfree queue we 'transfer' the
	// smart pointer into this object itself and use a raw
	// pointer inside the lockfree::queue.
	// When taking elements off the queue the smart pointer
	// is transferred back out of this object.
	static  void     take_ownership( BufChain p_owner );
	virtual BufChain yield_ownership() = 0;

	virtual Buf getHead()       = 0;
	virtual Buf getTail()       = 0;

	virtual unsigned getLen()   = 0;  // # of buffers in chain
	virtual size_t   getSize()  = 0; // in bytes

	virtual Buf createAtHead(size_t size, bool clip = false)  = 0;
	virtual Buf createAtTail(size_t size, bool clip = false)  = 0;

	virtual void addAtHead(Buf b) = 0;
	virtual void addAtTail(Buf b) = 0;

	// extract bufchain contents to 'buf' (starting at 'off' bytes
	// into the chain. At most 'size' bytes are stored to 'buf'.
	// RETURNS number of bytes copied.
	virtual uint64_t extract(void *buf, uint64_t off, uint64_t size) = 0;

	// insert data into bufchain. New buffers are appended as needed,
	// existing data are overwritten.
	virtual void     insert(void *buf, uint64_t off, uint64_t size)  = 0;

	virtual ~IBufChain(){}

	static BufChain create();
};

class IBufQueue {
public:
	virtual BufChain pop(const CTimeout *abs_timeout)                   = 0;
	virtual BufChain tryPop()                                           = 0;

	virtual bool     push(BufChain b, const CTimeout *abs_timeout)      = 0;
	virtual bool     tryPush(BufChain b)                                = 0;

	virtual CTimeout getAbsTimeoutPop(const CTimeout *rel_timeout)      = 0;
	virtual CTimeout getAbsTimeoutPush(const CTimeout *rel_timeout)     = 0;

	virtual IEventSource *getReadEventSource()                          = 0;
	virtual IEventSource *getWriteEventSource()                         = 0;

	virtual bool isFull()                                               = 0;
	virtual bool isEmpty()                                              = 0;

	virtual void shutdown()                                             = 0;
	virtual void startup()                                              = 0;

	virtual ~IBufQueue() {}


	static BufQueue create(unsigned size);
};

#endif
