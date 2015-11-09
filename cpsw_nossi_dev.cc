#include <cpsw_nossi_dev.h>
#include <stdint.h>

UdpAddress::UdpAddress(NoSsiDev *owner, unsigned short dport, unsigned timeoutUs, unsigned retryCnt)
:Address(owner), dport(dport), sd(-1), timeoutUs(timeoutUs), retryCnt(retryCnt)
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

	setTimeoutUs( timeoutUs );
}

UdpAddress::~UdpAddress()
{
	if ( -1 != sd )
		close( sd );
}

NoSsiDev::NoSsiDev(const char *name, const char *ip)
: Dev(name)
{
	if ( INADDR_NONE == ( d_ip = inet_addr( ip ) ) ) {
		throw InvalidArgError( ip );
	}
}

void UdpAddress::setTimeoutUs(unsigned timeoutUs)
{
struct timeval t;
	t.tv_sec  = timeoutUs / 1000000;
	t.tv_usec = timeoutUs % 1000000;
	if ( setsockopt( sd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t) ) ) {
		throw InternalError("setsocktop(SO_RECVTIMEO) failed");
	}
	this->timeoutUs = timeoutUs;
}

void UdpAddress::setRetryCount(unsigned retryCnt)
{
	this->retryCnt = retryCnt;
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
	
uint64_t UdpAddress::read(CompositePathIterator *node, Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
uint32_t bufh[4];
uint8_t  buft[4];
uint32_t xbuf[4];
uint32_t status;
int      i, j, put;
int      nw = (sbytes + 3)/4;
struct msghdr mh;
struct iovec  iov[4];
int      got;

	if ( nw == 0 )
		return 0;

	put = 0;
	xbuf[put++] = 0;
	xbuf[put++] = (off >> 2) & 0x3fffffff;
	xbuf[put++] = nw - 1;
	xbuf[put++] = 0;

	if ( BE == hostByteOrder() ) {
		for ( j=0; j<put; j++ ) {
			swp( (uint8_t*)&xbuf[j], sizeof(xbuf[j]));
		}
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
		// padding if not word-aligned
		iov[i].iov_base = buft;
		iov[i].iov_len  = 4 - (sbytes & 3);
		i++;
	}

	iov[i].iov_base = &status;
	iov[i].iov_len  = 4;
	i++;

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;


	unsigned attempt = 0;
	do {
		if ( (int)sizeof(xbuf[0])*put != write( sd, xbuf, sizeof(xbuf[0])*put ) ) {
			throw InternalError("FIXME -- need I/O Error here");
		}

		if ( (got = recvmsg( sd, &mh, 0 )) > 0 ) {
#if 0
			printf("got %i bytes, sbytes %i, nw %i\n", got, sbytes, nw);
				printf("got %i bytes\n", got);
				for (i=0; i<2; i++ )
					printf("header[%i]: %x\n", i, bufh[i]);

				for ( i=0; i<sbytes; i++ )
					printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			if ( got != (int)sizeof(bufh[0])*(nw + 3) ) {
				throw InternalError("FIXME -- need I/O Error here");
			}
			if ( BE == hostByteOrder() ) {
				swp( (uint8_t*)&status, sizeof(status) );
			}
			// TODO: check status word here
			return sbytes;
		}
	} while ( ++attempt <= retryCnt );

	throw InternalError("FIXME -- need I/O Error here");
}
