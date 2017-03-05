#ifndef CPSW_ASYNC_TRANSACTION_H
#define CPSW_ASYNC_TRANSACTION_H

#include <cpsw_buf.h>
#include <boost/shared_ptr.hpp>

class IAsyncIOCallback {
public:
	// if this is called with no rchn then a timeout occurred
	virtual void complete(BufChain rchn) = 0;

	virtual ~IAsyncIOCallback() {}
};

class IAsyncIOTransactionManager;
typedef shared_ptr<IAsyncIOTransactionManager> AsyncIOTransactionManager;

class IAsyncIOTransactionManager {
public:
	typedef uint32_t TID;	

	virtual void              post(IAsyncIOCallback *, TID) = 0;
	virtual IAsyncIOCallback *cancel(TID tid)               = 0;

	virtual ~IAsyncIOTransactionManager() {}

	static AsyncIOTransactionManager create(uint64_t timeoutUs = 500000);
};

#endif
