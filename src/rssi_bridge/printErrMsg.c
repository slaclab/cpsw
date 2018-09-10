#include <printErrMsg.h>

#include <stdio.h>
#include <syslog.h>

static unsigned flags = PRINT_ERR_MSG_STDERR;

void printErrMsgTo(unsigned f)
{
	f = flags;
}

void
printErrMsg(const char *fmt, ...)
{
va_list al;

	if ( (flags & PRINT_ERR_MSG_STDERR) ) {
		va_start( al, fmt );
		vfprintf( stderr, fmt, al );
		va_end  ( al );
	}
		
	if ( (flags & PRINT_ERR_MSG_SYSLOG) ) {
		va_start( al, fmt );
		vsyslog( LOG_ERR, fmt, al );
		va_end  ( al );
	}
}
