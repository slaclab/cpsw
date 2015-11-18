#include <cpsw_nossi_dev.h>
#include <stdint.h>

CUdpAddressImpl::CUdpAddressImpl(AKey k, unsigned short dport, unsigned timeoutUs, unsigned retryCnt)
:CAddressImpl(k), dport(dport), sd(-1), timeoutUs(timeoutUs), retryCnt(retryCnt)
{
struct sockaddr_in dst, me;
NoSsiDevImpl owner( getOwnerAs<NoSsiDevImpl>() );
	dst.sin_family      = AF_INET;
	dst.sin_port        = htons( dport );
	dst.sin_addr.s_addr = owner->getIpAddress();

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

CUdpAddressImpl::~CUdpAddressImpl()
{
	if ( -1 != sd )
		close( sd );
}

CNoSsiDevImpl::CNoSsiDevImpl(FKey k, const char *ip)
: CDevImpl(k), ip_str(ip)
{
	if ( INADDR_NONE == ( d_ip = inet_addr( ip ) ) ) {
		throw InvalidArgError( ip );
	}
}

void CUdpAddressImpl::setTimeoutUs(unsigned timeoutUs)
{
struct timeval t;
	t.tv_sec  = timeoutUs / 1000000;
	t.tv_usec = timeoutUs % 1000000;
	if ( setsockopt( sd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t) ) ) {
		throw InternalError("setsocktop(SO_RECVTIMEO) failed");
	}
	this->timeoutUs = timeoutUs;
}

void CUdpAddressImpl::setRetryCount(unsigned retryCnt)
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

#define CMD_READ  0x00000000
#define CMD_WRITE 0x40000000
	
uint64_t CUdpAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
uint32_t bufh[4];
uint8_t  buft[4];
uint32_t xbuf[4];
uint32_t status;
int      i, j, put;
int      headbytes = (off & 3);
int      totbytes;
struct msghdr mh;
struct iovec  iov[4];
int      got;
int      nw;

	if ( dbytes < sbytes )
		sbytes = dbytes;

	if ( sbytes == 0 )
		return 0;

	totbytes = headbytes + sbytes;

	nw = (totbytes + 3)/4;

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
	iov[i].iov_len  = 8 + headbytes;
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( totbytes & 3 ) {
		// padding if not word-aligned
		iov[i].iov_base = buft;
		iov[i].iov_len  = 4 - (totbytes & 3);
		i++;
	}

	iov[i].iov_base = &status;
	iov[i].iov_len  = 4;
	i++;

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;


	unsigned attempt = 0;
	do {
		if ( (int)sizeof(xbuf[0])*put != ::write( sd, xbuf, sizeof(xbuf[0])*put ) ) {
			throw IOError("Unable to send (complete) message");
		}

		if ( (got = ::recvmsg( sd, &mh, 0 )) > 0 ) {
#if 0
			printf("got %i bytes, sbytes %i, nw %i\n", got, sbytes, nw);
				printf("got %i bytes\n", got);
				for (i=0; i<2; i++ )
					printf("header[%i]: %x\n", i, bufh[i]);

				for ( i=0; i<sbytes; i++ )
					printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			if ( got != (int)sizeof(bufh[0])*(nw + 3) ) {
				throw IOError("Received message truncated");
			}
			if ( BE == hostByteOrder() ) {
				swp( (uint8_t*)&status, sizeof(status) );
			}
			// TODO: check status word here
			return sbytes;
		}
	} while ( ++attempt <= retryCnt );

	throw IOError("No response -- timeout");
}

uint64_t CUdpAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
uint32_t xbuf[4];
uint32_t status;
uint32_t zero = 0;
uint8_t  first_word[4];
uint8_t  last_word[4];
int      i, j, put;
int      headbytes = (off & 3);
int      totbytes;
struct msghdr mh;
struct iovec  iov[4];
int      got;
int      nw;

	if ( sbytes < dbytes )
		dbytes = sbytes;

	if ( dbytes == 0 )
		return 0;

	totbytes = headbytes + dbytes;

	nw = (totbytes + 3)/4;

	bool merge_first = headbytes      || msk1;
	bool merge_last  = (totbytes & 3) || mskn;

	int toput = dbytes;

	if ( merge_first ) {
		if ( merge_last && dbytes == 1 ) {
			// first and last byte overlap
			msk1 |= mskn;
			merge_last = false;
		}

		int first_byte = headbytes;

		read(node, cacheable, first_word, sizeof(first_word), off & ~3ULL, sizeof(first_word));

		first_word[ first_byte  ] = (first_word[ first_byte ] & msk1) | (src[0] & ~msk1);

		toput--;

		int remaining = (totbytes <= 4 ? totbytes : 4) - first_byte - 1;

		if ( merge_last && totbytes <= 4 ) {
			// last byte is also in first word
			int last_byte = (totbytes-1);
			first_word[ last_byte  ] = (first_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
			remaining--;
			toput--;
			merge_last = false;
		}

		if ( remaining > 0 ) {
			memcpy( first_word + first_byte + 1, src + 1, remaining );
			toput -= remaining;
		}

		if ( merge_last ) {
			read(node, cacheable, last_word, sizeof(last_word), (off + dbytes) & ~3ULL, sizeof(last_word));
			int last_byte = (totbytes-1) & 3;
			last_word[ last_byte  ] = (last_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
			toput--;

			if ( last_byte > 0 ) {
				memcpy(last_word, &src[dbytes-1-last_byte], last_byte);
				toput -= last_byte;
			}
		}
	}

	put = 0;
	xbuf[put++] = 0;
	xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;

	if ( BE == hostByteOrder() ) {
		for ( j=0; j<put; j++ ) {
			swp( (uint8_t*)&xbuf[j], sizeof(xbuf[j]));
		}

		swp( (uint8_t*)&zero, sizeof(zero) );
	}

	mh.msg_name       = 0;
	mh.msg_namelen    = 0;
	mh.msg_control    = 0;
	mh.msg_controllen = 0;
	mh.msg_flags      = 0;

	i = 0;
	iov[i].iov_base = xbuf;
	iov[i].iov_len  = 8;
	i++;

	if ( merge_first ) {
		iov[i].iov_base = first_word;
		iov[i].iov_len  = 4;
		i++;
	}

	if ( toput > 0 ) {
		iov[i].iov_base = src + (merge_first ? 4 - headbytes : 0);
		iov[i].iov_len  = toput;
		i++;
	}

	if ( merge_last ) {
		iov[i].iov_base = last_word;
		iov[i].iov_len  = 4;
		i++;
	}

	iov[i].iov_base = &zero;
	iov[i].iov_len  = 4;
	i++;

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;


	unsigned attempt = 0;
	do {
		int     bufsz = (nw+3)*4;
		uint8_t rbuf[bufsz];
		if ( bufsz != ::sendmsg( sd, &mh, 0 ) ) {
			throw InternalError("FIXME -- need I/O Error here");
		}

		if ( (got = ::read( sd, &rbuf, bufsz )) > 0 ) {
#if 0
			printf("got %i bytes, sbytes %i, nw %i\n", got, sbytes, nw);
				printf("got %i bytes\n", got);
				for (i=0; i<2; i++ )
					printf("header[%i]: %x\n", i, bufh[i]);

				for ( i=0; i<sbytes; i++ )
					printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			if ( got != bufsz ) {
				throw InternalError("FIXME -- need I/O Error here");
			}
			if ( BE == hostByteOrder() ) {
				swp( rbuf-4, 4 );
			}
			memcpy( &status, rbuf-4, 4 );
			// TODO: check status word here
			return dbytes;
		}
	} while ( ++attempt <= retryCnt );

	throw InternalError("FIXME -- need I/O Error here");
}

void CNoSsiDevImpl::addAtAddress(Field child, unsigned dport, unsigned timeoutUs, unsigned retryCnt)
{
IAddress::AKey k = getAKey();
	add( make_shared<CUdpAddressImpl>(k, dport, timeoutUs, retryCnt), child );
}

NoSsiDev INoSsiDev::create(const char *name, const char *ipaddr)
{
	return CEntryImpl::create<CNoSsiDevImpl>(name, ipaddr);
}
