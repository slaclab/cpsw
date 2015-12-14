#ifndef CPSW_BUF_H
#define CPSW_BUF_H

#include <boost/smart_ptr/shared_ptr.hpp>
#include <stdint.h>

using boost::shared_ptr;

class IBuf;
class IBufChain;

typedef shared_ptr<IBuf> Buf;
typedef shared_ptr<IBufChain> BufChain;

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
	virtual size_t   getCapacity()        = 0;
	virtual size_t   getSize()            = 0;
	virtual uint8_t *getPayload()         = 0;

	virtual void     setSize(size_t)      = 0;
	// setting payload ptr to 0 resets to start
	// of buffer
	virtual void     setPayload(uint8_t*) = 0;
	// reset payload ptr to start and size to capacity
	virtual void     reinit()             = 0;

	virtual Buf      getNext()            = 0;
	virtual Buf      getPrev()            = 0;

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

	static Buf       getBuf(size_t capa = 1500 - 14 - 20 - 8);

	// if no buffers are available on the free-list then
	// they are allocated from the heap. After use they go
	// to the free-list.
	static unsigned  numBufsAlloced(); // from heap
	static unsigned  numBufsFree();    // on free-list
	static unsigned  numBufsInUse();
};

class IBufChain {
public:

	virtual Buf getHead()       = 0;
	virtual Buf getTail()       = 0;

	virtual unsigned getLen()   = 0;  // # of buffers in chain
	virtual size_t   getSize()  = 0; // in bytes

	virtual Buf createAtHead()  = 0;
	virtual Buf createAtTail()  = 0;

	virtual void addAtHead(Buf b) = 0;
	virtual void addAtTail(Buf b) = 0;

	virtual ~IBufChain(){}

	static BufChain create();
};

#endif
