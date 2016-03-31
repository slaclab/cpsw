
CPSW_DIR=.

include $(CPSW_DIR)/defs.mak

HEADERS = cpsw_api_user.h cpsw_api_builder.h cpsw_api_timeout.h cpsw_error.h

cpsw_SRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
cpsw_SRCS+= cpsw_entry_adapt.cc
cpsw_SRCS+= cpsw_sval.cc
cpsw_SRCS+= cpsw_command.cc
cpsw_SRCS+= cpsw_mmio_dev.cc
cpsw_SRCS+= cpsw_mem_dev.cc
cpsw_SRCS+= cpsw_nossi_dev.cc
cpsw_SRCS+= cpsw_buf.cc
cpsw_SRCS+= cpsw_enum.cc
cpsw_SRCS+= cpsw_obj_cnt.cc
cpsw_SRCS+= cpsw_proto_mod.cc
cpsw_SRCS+= cpsw_proto_mod_depack.cc
cpsw_SRCS+= cpsw_proto_mod_udp.cc
cpsw_SRCS+= cpsw_proto_mod_srpmux.cc

STATIC_LIBRARIES+=cpsw

tstaux_SRCS+= crc32-le-tbl-4.c

STATIC_LIBRARIES+=tstaux

udpsrv_SRCS = udpsrv.c
udpsrv_SRCS+= udpsrv_mod_mem.cc
udpsrv_SRCS+= udpsrv_mod_axiprom.cc

udpsrv_LIBS = tstaux cpsw pthread

PROGRAMS   += udpsrv

cpsw_path_tst_SRCS       = cpsw_path_tst.cc
cpsw_path_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_path_tst

cpsw_shared_obj_tst_SRCS = cpsw_shared_obj_tst.cc
cpsw_shared_obj_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_shared_obj_tst

cpsw_sval_tst_SRCS       = cpsw_sval_tst.cc
cpsw_sval_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_sval_tst

cpsw_large_tst_SRCS      = cpsw_large_tst.cc
cpsw_large_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_large_tst

cpsw_nossi_tst_SRCS      = cpsw_nossi_tst.cc
cpsw_nossi_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_nossi_tst

cpsw_axiv_udp_tst_SRCS   = cpsw_axiv_udp_tst.cc
cpsw_axiv_udp_tst_LIBS   = $(CPSW_LIBS)
cpsw_axiv_udp_tst_LIBS  += yaml-cpp
YAML_VERSION=yaml-cpp-0.5.3
YAML_ARCH_linux_x86_64=rhel6-x86_64
YAML_ARCH_linuxRT_x86_64=buildroot-2015.02-x86_64
YAML_ARCH=$(YAML_ARCH_$(TARNM))
yaml_DIR = /afs/slac/g/lcls/package/yaml-cpp/$(YAML_VERSION)/$(YAML_ARCH)
yaml-cpp_DIR += $(yaml_DIR)/lib
INCLUDE_DIRS     += $(yaml_DIR)/include
TESTPROGRAMS            += cpsw_axiv_udp_tst

cpsw_buf_tst_SRCS        = cpsw_buf_tst.cc
cpsw_buf_tst_LIBS        = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_buf_tst

cpsw_stream_tst_SRCS     = cpsw_stream_tst.cc
cpsw_stream_tst_LIBS     = $(CPSW_LIBS)
cpsw_stream_tst_LIBS    += tstaux
TESTPROGRAMS            += cpsw_stream_tst

cpsw_srpmux_tst_SRCS     = cpsw_srpmux_tst.cc
cpsw_srpmux_tst_LIBS     = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_srpmux_tst

cpsw_enum_tst_SRCS     = cpsw_enum_tst.cc
cpsw_enum_tst_LIBS     = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_enum_tst


TEST_AXIV_YES=
TEST_AXIV_NO=cpsw_axiv_udp_tst
DISABLED_TESTPROGRAMS=$(TEST_AXIV_$(TEST_AXIV))

include $(CPSW_DIR)/rules.mak

# may override for individual test targets which will be
# executed with for all option sets, eg:
#
#    some_tst_run:RUN_OPTS='' '-a -b'
#
# (will run once w/o opts, a second time with -a -b)

# run for V2 and V1
cpsw_nossi_tst_run:     RUN_OPTS='' '-V1 -p8191'

cpsw_srpmux_tst_run:    RUN_OPTS='' '-V1 -p8191'

cpsw_axiv_udp_tst_run:  RUN_OPTS='' '-S 30'

# error percentage should be ~double of the value used for udpsrv (-L)
cpsw_stream_tst_run:    RUN_OPTS='-e 10'

cpsw_nossi_tst: udpsrv
