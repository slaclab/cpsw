#ifndef UDPSRV_REGDEFS_H
#define UDPSRV_REGDEFS_H

#define REGBASE 0x1000 /* pseudo register space */
#define REG_RO_OFF 0
#define REG_RO_SZ 16
#define REG_CLR_OFF REG_RO_SZ
#define REG_SCR_OFF (REG_CLR_OFF + 8)
#define REG_SCR_SZ  32
#define REG_ARR_OFF (REG_SCR_OFF + REG_SCR_SZ)
#define REG_ARR_SZ  8192

#endif

