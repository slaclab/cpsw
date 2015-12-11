#ifndef CPSW_BUF_H
#define CPSW_BUF_H

#include <boost/smart_ptr/shared_ptr.hpp>
#include <stdint.h>

using boost::shared_ptr;

class IBuf;
typedef shared_ptr<IBuf> Buf;

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
	virtual void     reset()              = 0;

	virtual Buf      getNext()            = 0;
	virtual Buf      getPrev()            = 0;

	// NOTE: none of the link manipulations
	//       are thread-safe
	// link after  this one
	virtual void     after(Buf)           = 0;
	// link before this one
	virtual void     before(Buf)          = 0;
	// unlink (remove this buffer from a list)
	virtual void     unlink()             = 0;
	// split (result are two lists with this buffer
	// at the head of the second one)
	virtual void     split()              = 0;

	static Buf       getBuf(size_t capa = 512*3);
};

#endif
