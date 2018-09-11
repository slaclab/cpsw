#ifndef UTIL_DAEMONIZE_H
#define UTIL_DAEMONIZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>

pid_t
daemonize();

#ifdef __cplusplus
}
#endif

#endif
