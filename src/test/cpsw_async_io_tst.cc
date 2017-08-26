#include <cpsw_api_user.h>
#include <cpsw_async_io.h>

#include <stdio.h>

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::shared_ptr;
using boost::make_shared;

class MyCallback : public CAsyncIOTransaction {
private:
	int id;
public:
	MyCallback(const CAsyncIOTransactionKey &key)
	: CAsyncIOTransaction( key )
	{
	}

	AsyncIOTransactionNode setId(int id)
	{
		this->id = id;
		return getSelfAsAsyncIOTransactionNode();
	}

	virtual void complete(BufChain rchn);

	virtual ~MyCallback() {}
};

void
MyCallback::complete(BufChain rchn)
{
	printf("Callback (id %i)\n", id);
}

typedef shared_ptr< CAsyncIOTransactionPool<MyCallback> > Pool;

int
main(int argc, char **argv)
{
AsyncIOTransactionManager mgr = IAsyncIOTransactionManager::create();
Pool                     pool = make_shared< CAsyncIOTransactionPool<MyCallback> >();
int                       i;

for ( i = 0; i<2; i++ ) {
	mgr->post( pool->alloc()->setId(1), 1, AsyncIO() );
	mgr->post( pool->alloc()->setId(2), 2, AsyncIO() );
	mgr->post( pool->alloc()->setId(3), 3, AsyncIO() );

	mgr->complete(BufChain(), 2);

	sleep(2);
}

	sleep(2);

	printf("Alloced %d, Free %d\n", pool->getNumAlloced(), pool->getNumFree());
}
