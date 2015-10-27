#include <api_user.h>
#include <ctype.h>

Entry::Entry(const char *cname, uint64_t sizeBits)
: name(cname), sizeBits(sizeBits)
{
const char *cptr;
	for ( cptr = cname; *cptr; cptr++ ) {
		if ( ! isalnum( *cptr )
                     && '_' != *cptr 
                     && '-' != *cptr  )
					throw InvalidIdentError(cname);
	}
}
