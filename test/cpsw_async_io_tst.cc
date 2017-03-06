#include <cpsw_api_user.h>
#include <cpsw_async_io.h>

#include <stdio.h>

class MyCallback : public CAsyncIOTransaction {
private:
	int id;
public:
	MyCallback(AsyncIOTransactionPool pool, const CAsyncIOTransactionKey &key)
	: CAsyncIOTransaction( pool, key )
	{
	}

	MyCallback * setId(int id)
	{
		this->id = id;
		return this;
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
Pool                     pool = CAsyncIOTransactionPool<MyCallback>::create();

	mgr->post( pool->getTransaction()->setId(1), 1 );
	mgr->post( pool->getTransaction()->setId(2), 2 );
	mgr->post( pool->getTransaction()->setId(3), 2 );

	mgr->complete(BufChain(), 2);

	sleep(4);
}
