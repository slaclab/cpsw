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

	// check release of a chain:
	b[0] = IBuf::getBuf();
	for ( i=1; i<NBUF; i++ )
		IBuf::getBuf()->after( b[0] );
	Buf p;
	for ( p = b[0], i=0; p; p = p->getNext(), i++ ) {
		p->getPayload()[0] = (uint8_t)i;
	}
	if ( i != NBUF )
		throw TestFailed("building chain failed");

	if (   IBuf::numBufsAlloced() != NBUF
		|| IBuf::numBufsFree()    != 0
		|| IBuf::numBufsInUse()   != NBUF )
		throw TestFailed("buffer count after chain creation wrong");


	b[1] = b[0]->getNext();

	b[0].reset();
	if ( b[1]->getPrev() )
		throw TestFailed("PREV should be NULL here");
	b[1].reset();

	if (   IBuf::numBufsAlloced() != NBUF
		|| IBuf::numBufsFree()    != NBUF
		|| IBuf::numBufsInUse()   != 0 )
		throw TestFailed("buffer count after chain destruction wrong");

	// test chains
	BufChain ch0 = IBufChain::create();
	b[0] = ch0->createAtHead();
	b[1] = ch0->createAtTail();

	size_t s_orig = ch0->getSize();

	if ( 2 != ch0->getLen() )
		throw TestFailed("buffer length test 1 failed");

	b[1]->setSize( b[1]->getSize() - 20 );
	b[0]->setPayload( b[0]->getPayload() + 13 );

	if ( ch0->getSize() != s_orig -20 -13 )
		throw TestFailed("buffer size test 1 failed");

	b[2] = IBuf::getBuf();
	b[2]->setSize( 53 );

	b[2]->before( b[0] );
	if ( ch0->getHead() != b[2] )
		throw TestFailed("inserting buffer at head failed");

	b[3] = IBuf::getBuf();
	b[3]->setSize( 127 );

	b[3]->after( b[1] );
	if ( ch0->getTail() != b[3] )
		throw TestFailed("inserting buffer at head failed");

	if ( 4 != ch0->getLen() )
		throw TestFailed("buffer length test 2 failed");

	if ( s_orig + b[2]->getSize() + b[3]->getSize() - 20 - 13 != ch0->getSize() )
		throw TestFailed("buffer size test 2 failed");
		
	try {
		b[1]->split();
		throw TestFailed("splitting a chain should fail!");
	} catch (InternalError e) {
	}

	b[1]->unlink();
	if ( ch0->getHead() != b[2] || ch0->getTail() != b[3] )
		throw TestFailed("chain inconsistency 1");

	if ( b[1]->getNext() || b[1]->getPrev() || b[1]->getChain() )
		throw TestFailed("buffer inconsistency after unlink() 1");

	if ( 3 != ch0->getLen() || b[0]->getSize() + b[2]->getSize() + b[3]->getSize() != ch0->getSize() )
		throw TestFailed("buffer size/len test 3 failed");

	
	if ( ch0->getHead() != b[2] || ch0->getTail() != b[3] )
		throw TestFailed("chain inconsistency 2");

	ch0->getHead()->unlink();
	
	if ( ch0->getHead() != b[0] || ch0->getTail() != b[3] )
		throw TestFailed("chain inconsistency 3");

	if ( 2 != ch0->getLen() || b[0]->getSize() + b[3]->getSize() != ch0->getSize() )
		throw TestFailed("buffer size/len test 4 failed");

	b[3]->unlink();
	if ( b[3]->getNext() || b[3]->getPrev() || b[3]->getChain() )
		throw TestFailed("buffer inconsistency after unlink() 2");

	if ( ch0->getHead() != b[0] || ch0->getTail() != b[0] )
		throw TestFailed("chain inconsistency 4");

	if ( 1 != ch0->getLen() || b[0]->getSize() != ch0->getSize() )
		throw TestFailed("buffer size/len test 5 failed");

	ch0->getHead()->unlink();

	if ( b[0]->getNext() || b[0]->getPrev() || b[0]->getChain() )
		throw TestFailed("buffer inconsistency after unlink() 3");

	if ( ch0->getHead() || ch0->getTail() )
		throw TestFailed("chain inconsistency 5");

	if ( ch0->getLen() != 0 || ch0->getSize() != 0 )
		throw TestFailed("chain size/len test 6 failed");

	for ( i=0; i<=3; i++ ) {
		// attach all bufs to chain
		ch0->addAtHead( b[i] );
		// release refs
		b[i].reset();
	}

	// releasing ref to the chain should clean everything up
	ch0.reset();

	if (   IBuf::numBufsAlloced() != NBUF
		|| IBuf::numBufsFree()    != NBUF
		|| IBuf::numBufsInUse()   != 0 )
		throw TestFailed("buffer count after chain destruction (2) wrong");

	
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
