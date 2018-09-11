#ifndef UTIL_PRINT_ERR_MSG_H
#define UTIL_PRINT_ERR_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#define PRINT_ERR_MSG_STDERR (1<<0)
#define PRINT_ERR_MSG_SYSLOG (1<<1)

/* May be a combination of destinations; by 
 * default only STDERR is enabled.
 */
void printErrMsgTo(unsigned f);

void
printErrMsg(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
