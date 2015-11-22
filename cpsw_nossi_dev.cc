#include <cpsw_nossi_dev.h>
#include <inttypes.h>

typedef shared_ptr<NoSsiDevImpl> NoSsiDevImplP;

typedef uint32_t SRPWord;

#undef  NOSSI_DEBUG

UdpAddressImpl::UdpAddressImpl(AKey k, NoSsiDevImpl::ProtocolVersion version, unsigned short dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc)
:AddressImpl(k),
 protoVersion(version),
 dport(dport),
 sd(-1),
 timeoutUs(timeoutUs),
 retryCnt(retryCnt),
 vc(vc),
 tid(0)
{
struct sockaddr_in dst, me;
NoSsiDevImplP owner( getOwnerAs<NoSsiDevImplP>() );
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

UdpAddressImpl::~UdpAddressImpl()
{
	if ( -1 != sd )
		close( sd );
}

NoSsiDevImpl::NoSsiDevImpl(FKey k, const char *ip)
: DevImpl(k), ip_str(ip)
{
	if ( INADDR_NONE == ( d_ip = inet_addr( ip ) ) ) {
		throw InvalidArgError( ip );
	}
}

void UdpAddressImpl::setTimeoutUs(unsigned timeoutUs)
{
struct timeval t;
	t.tv_sec  = timeoutUs / 1000000;
	t.tv_usec = timeoutUs % 1000000;
	if ( setsockopt( sd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t) ) ) {
		throw InternalError("setsocktop(SO_RECVTIMEO) failed");
	}
	this->timeoutUs = timeoutUs;
}

void UdpAddressImpl::setRetryCount(unsigned retryCnt)
{
	this->retryCnt = retryCnt;
}

static void swp32(SRPWord *buf)
{
#ifdef __GNUC__
	*buf = __builtin_bswap32( *buf );
#else
	#error "bswap32 needs to be implemented"
#endif
}

static void swpw(uint8_t *buf)
{
uint32_t v;
	// gcc knows how to optimize this
	memcpy( &v, buf, sizeof(SRPWord) );
#ifdef __GNUC__
	v = __builtin_bswap32( v );
#else
	#error "bswap32 needs to be implemented"
#endif
	memcpy( buf, &v, sizeof(SRPWord) );
}

#define CMD_READ  0x00000000
#define CMD_WRITE 0x40000000
	
uint64_t UdpAddressImpl::read(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *dst, unsigned dbytes, uint64_t off, unsigned sbytes) const
{
SRPWord    bufh[4];
uint8_t  buft[sizeof(SRPWord)];
SRPWord    xbuf[5];
SRPWord    header;
SRPWord    status;
int      i, j, put;
int      headbytes = (off & (sizeof(SRPWord)-1));
int      tailbytes = 0;
int      totbytes;
struct msghdr mh;
struct iovec  iov[5];
int      got;
int      nWords;
int      expected = 0;
uint32_t tid = getTid();

	if ( dbytes < sbytes )
		sbytes = dbytes;

	if ( sbytes == 0 )
		return 0;

	// V1 sends payload in network byte order. We want to transform to restore
	// the standard AXI layout which is little-endian
	bool doSwapV1 = (INoSsiDev::SRP_UDP_V1 == protoVersion);
	// header info needs to be swapped to host byte order since we interpret it
	bool doSwap   =	( ( protoVersion == INoSsiDev::SRP_UDP_V2 ? BE : LE ) == hostByteOrder() );

	totbytes = headbytes + sbytes;

	nWords   = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	put = expected = 0;
	if ( protoVersion == INoSsiDev::SRP_UDP_V1 ) {
		xbuf[put++] = vc << 24;
		expected++;
	}
	xbuf[put++] = tid;
	xbuf[put++] = (off >> 2) & 0x3fffffff;
	xbuf[put++] = nWords   - 1;
	xbuf[put++] = 0;
	expected += 3;

	// V2 uses LE, V1 network (aka BE) layout
	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}
	}

	mh.msg_name       = 0;
	mh.msg_namelen    = 0;
	mh.msg_control    = 0;
	mh.msg_controllen = 0;
	mh.msg_flags      = 0;

	i = 0;
	if ( protoVersion == INoSsiDev::SRP_UDP_V1 ) {
		iov[i].iov_base = &header;
		iov[i].iov_len  = sizeof( header );
		i++;
	}
	iov[i].iov_base = bufh;
	iov[i].iov_len  = 8 + headbytes;
	i++;

	iov[i].iov_base = dst;
	iov[i].iov_len  = sbytes;
	i++;

	if ( totbytes & (sizeof(SRPWord)-1) ) {
		tailbytes = sizeof(SRPWord) - (totbytes & (sizeof(SRPWord)-1));
		// padding if not word-aligned
		iov[i].iov_base = buft;
		iov[i].iov_len  = tailbytes;
		i++;
	}

	iov[i].iov_base = &status;
	iov[i].iov_len  = sizeof(SRPWord);
	i++;

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;


	unsigned attempt = 0;
	do {
		if ( (int)sizeof(xbuf[0])*put != ::write( sd, xbuf, sizeof(xbuf[0])*put ) ) {
			throw IOError("Unable to send (complete) message");
		}

		do {
			if ( (got = ::recvmsg( sd, &mh, 0 )) < 0 ) {
				goto retry;
			}
#ifdef NOSSI_DEBUG
			printf("got %i bytes, off 0x%"PRIx64", sbytes %i, nWords %i\n", got, off, sbytes, nWords  );
			printf("got %i bytes\n", got);
			for (i=0; i<2; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);
			for ( i=0; i<headbytes; i++ ) {
				printf("headbyte[%i]: %x\n", i, ((uint8_t*)&bufh[2])[i]);
			}

			for ( i=0; (unsigned)i<sbytes; i++ ) {
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
			}

			for ( i=0; i < tailbytes; i++ ) {
				printf("tailbyte[%i]: %x\n", i, buft[i]);
			}
#endif
			if ( doSwap ) {
				swp32( &bufh[0] );
			}
		} while ( bufh[0] != tid );

		if ( got != (int)sizeof(bufh[0])*(nWords + expected) ) {
			printf("got %i, nw %i, exp %i\n", got, nWords, expected);
			throw IOError("Received message truncated");
		}
		if ( doSwapV1 ) {
			// switch payload back to LE
			uint8_t  tmp[sizeof(SRPWord)];
			unsigned hoff = 0;
			if ( headbytes ) {
				hoff += sizeof(SRPWord) - headbytes;
				memcpy(tmp, &bufh[2], headbytes);
				if ( hoff > sbytes ) {
					// special case where a single word covers bufh, dst and buft
					memcpy(tmp+headbytes,        dst , sbytes);
					// note: since headbytes < 4 -> hoff < 4
					//       and since sbytes < hoff -> sbytes < 4 -> sbytes == sbytes % 4
					//       then we have: headbytes = off % 4
					//                     totbytes  = off % 4 + sbytes  and totbytes % 4 == (off + sbytes) % 4 == headbytes + sbytes
					//                     tailbytes =  4 - (totbytes % 4)
					//       and thus: headbytes + tailbytes + sbytes  == 4
					memcpy(tmp+headbytes+sbytes, buft, tailbytes);
				} else {
					memcpy(tmp+headbytes, dst, hoff);
				}
#ifdef NOSSI_DEBUG
				for (i=0; i< (int)sizeof(SRPWord); i++) printf("headbytes tmp[%i]: %x\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst, tmp+headbytes, hoff);
			}
			int jend =(((int)(sbytes - hoff)) & ~(sizeof(SRPWord)-1));
			for ( j = 0; j < jend; j+=sizeof(SRPWord) ) {
				swpw( dst + hoff + j );
			}
			if ( tailbytes && (hoff <= sbytes) ) { // cover the special case mentioned above
				memcpy(tmp, dst + hoff + j, sizeof(SRPWord) - tailbytes);
				memcpy(tmp + sizeof(SRPWord) - tailbytes, buft, tailbytes);
#ifdef NOSSI_DEBUG
				for (i=0; i< (int)sizeof(SRPWord); i++) printf("tailbytes tmp[%i]: %"PRIx8"\n", i, tmp[i]);
#endif
				swpw( tmp );
				memcpy(dst + hoff + j, tmp, sizeof(SRPWord) - tailbytes);
			}
#ifdef NOSSI_DEBUG
			for ( i=0; i< (int)sbytes; i++ ) printf("swapped dst[%i]: %x\n", i, dst[i]);
#endif
		}
		if ( doSwap ) {
			swp32( &status );
			swp32( &header );
		}
		// TODO: check status word here
		return sbytes;

retry: ;
	} while ( ++attempt <= retryCnt );

	throw IOError("No response -- timeout");
}

uint64_t UdpAddressImpl::write(CompositePathIterator *node, IField::Cacheable cacheable, uint8_t *src, unsigned sbytes, uint64_t off, unsigned dbytes, uint8_t msk1, uint8_t mskn) const
{
	SRPWord    xbuf[4];
	SRPWord    status;
	SRPWord    header;
	SRPWord    zero = 0;
	uint8_t  first_word[sizeof(SRPWord)];
	uint8_t  last_word[sizeof(SRPWord)];
	int      i, j, put;
	int      headbytes = (off & (sizeof(SRPWord)-1));
	int      totbytes;
	struct msghdr mh;
	struct iovec  iov[5];
	int      got;
	int      nWords;
	uint32_t tid = getTid();

	if ( sbytes < dbytes )
		dbytes = sbytes;

	if ( dbytes == 0 )
		return 0;

	// these look similar but are different...
	bool doSwapV1 = (protoVersion == INoSsiDev::SRP_UDP_V1);
	bool doSwap   = ((protoVersion == INoSsiDev::SRP_UDP_V2 ? BE : LE) == hostByteOrder() );

	totbytes = headbytes + dbytes;

	nWords = (totbytes + sizeof(SRPWord) - 1)/sizeof(SRPWord);

	bool merge_first = headbytes      || msk1;
	bool merge_last  = (totbytes & (sizeof(SRPWord)-1)) || mskn;

	int toput = dbytes;

	if ( merge_first ) {
		if ( merge_last && dbytes == 1 ) {
			// first and last byte overlap
			msk1 |= mskn;
			merge_last = false;
		}

		int first_byte = headbytes;

		read(node, cacheable, first_word, sizeof(first_word), off & ~3ULL, sizeof(first_word));

		if ( doSwapV1 )
			swpw( first_word );

		first_word[ first_byte  ] = (first_word[ first_byte ] & msk1) | (src[0] & ~msk1);

		toput--;

		int remaining = (totbytes <= (int)sizeof(SRPWord) ? totbytes : sizeof(SRPWord)) - first_byte - 1;

		if ( merge_last && totbytes <= (int)sizeof(SRPWord) ) {
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
	}
	if ( merge_last ) {

		read(node, cacheable, last_word, sizeof(last_word), (off + dbytes) & ~3ULL, sizeof(last_word));

		if ( doSwapV1 )
			swpw( last_word );

		int last_byte = (totbytes-1) & (sizeof(SRPWord)-1);
		last_word[ last_byte  ] = (last_word[ last_byte ] & mskn) | (src[dbytes - 1] & ~mskn);
		toput--;

		if ( last_byte > 0 ) {
			memcpy(last_word, &src[dbytes-1-last_byte], last_byte);
			toput -= last_byte;
		}
	}

	put = 0;
	if ( protoVersion == INoSsiDev::SRP_UDP_V1 ) {
		xbuf[put++] = vc << 24;
	}
	xbuf[put++] = tid;
	xbuf[put++] = ((off >> 2) & 0x3fffffff) | CMD_WRITE;

	if ( doSwap ) {
		for ( j=0; j<put; j++ ) {
			swp32( &xbuf[j] );
		}

		swp32( &zero );
	}

	mh.msg_name       = 0;
	mh.msg_namelen    = 0;
	mh.msg_control    = 0;
	mh.msg_controllen = 0;
	mh.msg_flags      = 0;

	i = 0;
	iov[i].iov_base = xbuf;
	iov[i].iov_len  = sizeof(xbuf[0])*put;
	i++;

	if ( merge_first ) {
		if ( doSwapV1 ) 
			swpw( first_word );
		iov[i].iov_base = first_word;
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	}

	if ( toput > 0 ) {
		iov[i].iov_base = src + (merge_first ? sizeof(SRPWord) - headbytes : 0);
		iov[i].iov_len  = toput;

		if ( doSwapV1 ) {
			for ( j = 0; j<toput; j += sizeof(SRPWord) ) {
				swpw( (uint8_t*)iov[i].iov_base + j );
			}
		}
		i++;
	}

	if ( merge_last ) {
		if ( doSwapV1 )
			swpw( last_word );
		iov[i].iov_base = last_word;
		iov[i].iov_len  = sizeof(SRPWord);
		i++;
	}

	iov[i].iov_base = &zero;
	iov[i].iov_len  = sizeof(SRPWord);
	i++;

	mh.msg_iov        = iov;
	mh.msg_iovlen     = i;


	unsigned attempt = 0;
	uint32_t got_tid;

	do {
		int     bufsz = (nWords + 3)*sizeof(SRPWord);
		if ( protoVersion == INoSsiDev::SRP_UDP_V1 ) {
			bufsz += sizeof(SRPWord);
		}
		uint8_t rbuf[bufsz];

		if ( bufsz != ::sendmsg( sd, &mh, 0 )) {
			throw InternalError("FIXME -- need I/O Error here");
		}

		do {
			if ( (got = ::read( sd, &rbuf, bufsz )) < 0 ) {
				goto retry;
			}
#if 0
			printf("got %i bytes, sbytes %i, nWords %i\n", got, sbytes, nWords);
			printf("got %i bytes\n", got);
			for (i=0; i<2; i++ )
				printf("header[%i]: %x\n", i, bufh[i]);

			for ( i=0; i<sbytes; i++ )
				printf("chr[%i]: %x %c\n", i, dst[i], dst[i]);
#endif
			memcpy( &got_tid, rbuf + ( protoVersion == INoSsiDev::SRP_UDP_V1 ? sizeof(SRPWord) : 0 ), sizeof(SRPWord) );
			if ( doSwap ) {
				swp32( &got_tid ); 
			}
		} while ( tid != got_tid );

		if ( got != bufsz ) {
			throw InternalError("FIXME -- need I/O Error here");
		}
		if ( protoVersion == INoSsiDev::SRP_UDP_V1 ) {
			if ( LE == hostByteOrder() ) {
				swpw( rbuf );
			}
			memcpy( &header, rbuf,  sizeof(SRPWord) );
		} else {
			header = 0;
		}
		if ( doSwap ) {
			swpw( rbuf + got - sizeof(SRPWord) );
		}
		memcpy( &status, rbuf + got - sizeof(SRPWord), sizeof(SRPWord) );
		// TODO: check status word here
		return dbytes;
retry: ;
	} while ( ++attempt <= retryCnt );

	throw InternalError("FIXME -- need I/O Error here");
}

void NoSsiDevImpl::addAtAddress(Field child, ProtocolVersion version, unsigned dport, unsigned timeoutUs, unsigned retryCnt, uint8_t vc)
{
AKey k = getAKey();
	add( make_shared<UdpAddressImpl>(k, version, dport, timeoutUs, retryCnt, vc), child );
}

NoSsiDev INoSsiDev::create(const char *name, const char *ipaddr)
{
	return EntryImpl::create<NoSsiDevImpl>(name, ipaddr);
}
