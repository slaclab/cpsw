#include <stdio.h>
#include <cpsw_api_user.h>
#include <cpsw_error.h>

#include <cpsw_buf.h>

#define NBUF 4

class TestFailed {
public:
	string e_;
	TestFailed(const char *e):e_(e) {}
};

void checkListCons(const char * ctxt, Buf p, unsigned ibeg, unsigned iend)
{
unsigned i;
Buf pp;
	try {
		for ( i=ibeg; p; i++, p=p->getNext() ) {
			if ( *p->getPayload() != (uint8_t) i ) {
				printf("got %i, exp %i\n", *p->getPayload(), i);
				throw TestFailed("list inconsistency (running forward)");	
			}
			pp = p;
		}
		if ( i != iend )
			throw TestFailed("list inconsistency at end");	
		for ( p=pp; p; p=p->getPrev() ) {
			--i;
			if ( *p->getPayload() != (uint8_t) i )
				throw TestFailed("list inconsistency (running backwards)");	
			pp = p;
		}
		if ( i != ibeg ) {
		printf("ibeg %i; i %i\n", ibeg, i);
			throw TestFailed("list inconsistency at head");	
		}
	} catch ( TestFailed e ) {
		fprintf(stderr, "%s -- ", ctxt);
		throw;
	}
}

int
main(int argc, char **argv)
{
Buf      b[NBUF];
unsigned i;

try {
	for ( i=0; i<NBUF; i++ )
		b[i] = IBuf::getBuf();

	if ( b[0]->getCapacity() != b[0]->getSize() )
		throw TestFailed("initial size mismatch");

	if (   IBuf::numBufsAlloced() != NBUF
		|| IBuf::numBufsFree()    != 0
		|| IBuf::numBufsInUse()   != NBUF ) {
		throw TestFailed("initial buffer count wrong");
	}

	// play with payload + size
	uint8_t *op;
	b[0]->setPayload( (op = b[0]->getPayload()) + 4 );
	b[0]->setSize( 8 );
	if ( b[0]->getSize() != 8 )
		throw TestFailed("changing 'size' failed");
	if ( b[0]->getPayload() - op != 4 ) {
		throw TestFailed("changing 'payload' failed");
	}
	b[0]->reinit();
	if ( b[0]->getPayload() != op || b[0]->getSize() != b[0]->getCapacity() )
		throw TestFailed("reinit failed");

	if ( b[0]->getNext() || b[0]->getPrev() )
		throw TestFailed("prev/next initiall non-NULL");

	for ( i=0; i<NBUF; i++ )
		*(b[i]->getPayload()) = (uint8_t)i;

	for ( i=1; i<NBUF; i++ ) {
		b[i]->after( b[i-1] );
	}

	checkListCons("Testing 'after'", b[0], 0, NBUF );

	unsigned j = NBUF/2;

	for ( i=j; i<NBUF; i++ )
		b[i]->unlink();
	for ( i=0; i<j; i++ )
		b[i]->unlink();

	for ( i=0; i<NBUF; i++ ) {
		if ( b[i]->getNext() || b[i]->getPrev() )
			throw TestFailed("unlink failed");
	}

	for ( i=NBUF-1; i>0; i-- ) {
		b[i-1]->before( b[i] );
	}

	checkListCons("Testing 'before'", b[0], 0, NBUF );

	b[j]->split();

	checkListCons("split -- first  part", b[0], 0, j);

	checkListCons("split -- second part", b[j], j, NBUF);

	for ( i=1; i<j; i++ )
		b[i]->split();

	for ( i=j+1; i<NBUF; i++ )
		b[i]->split();

	for ( i=0; i<NBUF; i++ )
		b[i].reset();

	if (   IBuf::numBufsAlloced() != NBUF
		|| IBuf::numBufsFree()    != NBUF
		|| IBuf::numBufsInUse()   != 0 )
		throw TestFailed("final buffer count wrong");

	

	
} catch ( CPSWError e ) {
	fprintf(stderr,"ERROR: %s\n", e.getInfo().c_str());
	throw;
} catch ( TestFailed e) {
	fprintf(stderr,"TEST FAILED: %s\n", e.e_.c_str());
	throw;
}
	printf("CPSW Buffer test PASSED\n");	
	return 0;
}
