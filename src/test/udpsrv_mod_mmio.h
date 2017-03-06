#ifndef UDPSRV_MOD_MMIO_H
#define UDPSRV_MOD_MMIO_H

#ifdef __cplusplus
extern "C" {
#endif

int register_io_range(const char *sysfs_name, uint64_t map_start);

#ifdef __cplusplus
};
#endif

#endif
