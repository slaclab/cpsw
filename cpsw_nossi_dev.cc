#include <cpsw_nossi_dev.h>
#include <stdint.h>

UdpAddress::UdpAddress(NoSsiDev *owner, unsigned short dport)
:Address(owner), dport(dport), sd(-1)
{
struct sockaddr_in dst, me;
	dst.sin_family      = AF_INET;
	dst.sin_port        = htons( dport );
	dst.sin_addr.s_addr = owner->getIp();

	me.sin_family       = AF_INET;
	me.sin_port         = htons( 0 );
	me.sin_addr.s_addr  = INADDR_ANY;

	if ( (sd = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 ) {
		throw InvalidArgError("Unable to create socket");
	}

	if ( bind( sd, (struct sockaddr*)&me, sizeof( me ) ) ) {
		throw InternalError("Unable to bind socket");
	}

	if ( connect( sd, (struct sockaddr*)&dst, sizeof( dst ) ) ) {
		throw InvalidArgError("Unable to connect socket");
	}
}

UdpAddress::~UdpAddress()
{
	if ( -1 != sd )
		close( sd );
}

NoSsiDev::NoSsiDev(const char *name, const char *ip) : Dev(name)
{
	if ( INADDR_NONE == ( d_ip = inet_addr( ip ) ) ) {
		throw InvalidArgError( ip );
	}
}

static void swp(uint8_t *buf, size_t sz)
{
unsigned int k;
	for (k=0; k<sz/2; k++ ) {
				uint8_t tmp = buf[k];
				buf[k]      = buf[sz-1-k];
				buf[sz-1-k] = tmp;
	}
}
	
uint64_t UdpAddress::read(CompositePathIterator *node, bool cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
uint32_t bufh[10];
uint8_t  buft[4];
int      i=0, j;
int      nw = (sbytes + 3)/4;
struct msghdr mh;
struct iovec  iov[3];

	if ( nw == 0 )
		return 0;

	bufh[i++] = 0;
	bufh[i++] = (off >> 2) & 0x3fffffff;
	bufh[i++] = nw - 1;
	bufh[i++] = 0;

	if ( BE == hostByteOrder() ) {
		for ( j=0; j<i; j++ ) {
			swp( (uint8_t*)&bufh[j], sizeof(bufh[j]));
		}
	}

	if ( (int)sizeof(bufh[0])*i != write( sd, bufh, sizeof(bufh[0])*i ) ) {
		throw InternalError("FIXME -- need I/O Error here");
	}

	mh.msg_name       = 0;
	mh.msg_namelen    = 0;
	mh.msg_control    = 0;
	mh.msg_controllen = 0;
	mh.msg_flags      = 0;

	i = 0;
	iov[i].iov_base = bufh;
	iov[i].iov_len  = 8 + (off & 3);
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( sbytes & 3 ) {
		iov[i].iov_base = buft;
		iov[i].iov_len  = 4 - (sbytes & 3);
		i++;
	}

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;

	int got;

	bufh[0] = 0xdeadbeef;
	bufh[1] = 0xdeadbeef;

	if ( (got = recvmsg( sd, &mh, 0 )) < 0 ) {
		throw InternalError("FIXME -- need I/O Error here");
	}

//	printf("got %i bytes\n", got);
//	for (i=0; i<2; i++ )
//		printf("header[%i]: %x\n", i, bufh[i]);

//	for ( i=0; i<sbytes; i++ )
//		printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
	
	return sbytes;
}
