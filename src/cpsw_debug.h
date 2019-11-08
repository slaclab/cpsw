#ifndef CPSW_DEBUGGING_LEVELS_H
#define CPSW_DEBUGGING_LEVELS_H

#include <iostream>
#include <stdio.h>

#include <cpsw_stdio.h>

extern     int cpsw_rssi_debug   __attribute__((weak));
extern     int cpsw_psbldr_debug __attribute__((weak));
extern     int cpsw_thread_debug __attribute__((weak));
extern "C" int libSocksDebug     __attribute__((weak));

#endif
