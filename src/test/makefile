# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

CPSW_DIR=..

include $(CPSW_DIR)/defs.mak

DEP_HEADERS += crc32-le-tbl-4.h
DEP_HEADERS += udpsrv_regdefs.h
DEP_HEADERS += udpsrv_port.h
DEP_HEADERS += udpsrv_rssi_port.h
DEP_HEADERS += udpsrv_stream_state_monitor.h
DEP_HEADERS += udpsrv_util.h
DEP_HEADERS += udpsrv_cutil.h

tstaux_SRCS+= crc32-le-tbl-4.c

tstauxlib_DIR=.

STATIC_LIBRARIES_YES+=tstaux
SHARED_LIBRARIES_YES+=tstaux

udpsrv_SRCS = udpsrv.c
udpsrv_SRCS+= udpsrv_port.cc
udpsrv_SRCS+= udpsrv_util.cc
udpsrv_SRCS+= udpsrv_mod_mem.cc
udpsrv_SRCS+= udpsrv_mod_axiprom.cc
udpsrv_SRCS+= udpsrv_mod_pendsim.cc
udpsrv_SRCS+= udpsrv_mod_genadc.cc
udpsrv_SRCS+= udpsrv_mod_mmio.cc
udpsrv_SRCS+= udpsrv_stream_state_monitor.cc

udpsrv_CXXFLAGS+= -DUDPSRV

udpsrv_LIBS_NO = tstaux $(CPSW_LIBS)
udpsrv_LIBS_YES= tstaux.a cpsw.a pthread rt
udpsrv_LIBS=$(udpsrv_LIBS_$(WITH_STATIC_LIBRARIES))

rssi_bridge_SRCS = rssi_bridge.cc
rssi_bridge_SRCS+= udpsrv_util.cc
rssi_bridge_SRCS+= udpsrv_stream_state_monitor.cc

rssi_bridge_LIBS_NO = $(CPSW_LIBS)
rssi_bridge_LIBS_YES= cpsw.a pthread rt
rssi_bridge_LIBS=$(rssi_bridge_LIBS_$(WITH_STATIC_LIBRARIES))


cpsw_path_tst_SRCS       = cpsw_path_tst.cc
cpsw_path_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_path_tst

cpsw_shared_obj_tst_SRCS = cpsw_shared_obj_tst.cc
cpsw_shared_obj_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_shared_obj_tst

cpsw_sval_tst_SRCS       = cpsw_sval_tst.cc
cpsw_sval_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_sval_tst

cpsw_const_sval_tst_SRCS = cpsw_const_sval_tst.cc
cpsw_const_sval_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_const_sval_tst


cpsw_large_tst_SRCS      = cpsw_large_tst.cc
cpsw_large_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_large_tst

cpsw_netio_tst_SRCS      = cpsw_netio_tst.cc
cpsw_netio_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_netio_tst

cpsw_axiv_udp_tst_SRCS   = cpsw_axiv_udp_tst.cc
cpsw_axiv_udp_tst_LIBS   = $(CPSW_LIBS)
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

cpsw_enum_tst_SRCS       = cpsw_enum_tst.cc
cpsw_enum_tst_LIBS       = $(CPSW_LIBS)
TESTPROGRAMS            += cpsw_enum_tst

rssi_tst_SRCS            = rssi_tst.cc udpsrv_port.cc udpsrv_util.cc udpsrv_stream_state_monitor.cc
rssi_tst_LIBS            = $(CPSW_LIBS)
TESTPROGRAMS            += rssi_tst

cpsw_srpv3_large_tst_SRCS += cpsw_srpv3_large_tst.cc
cpsw_srpv3_large_tst_LIBS += $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_srpv3_large_tst

cpsw_command_tst_SRCS      = cpsw_command_tst.cc
cpsw_command_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_command_tst

MyCommand_so_SRCS          = cpsw_myCommand.cc
MyCommand_so_LDFLAGS       = -shared
MyCommand_so_CXXFLAGS      = -fpic
TESTPROGRAMS              += MyCommand.so

cpsw_yaml_preproc_tst_SRCS = cpsw_yaml_preproc_tst.cc
cpsw_yaml_preproc_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_yaml_preproc_tst

cpsw_yaml_merge_tst_SRCS = cpsw_yaml_merge_tst.cc
cpsw_yaml_merge_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_yaml_merge_tst

cpsw_yaml_config_tst_SRCS = cpsw_yaml_config_tst.cc
cpsw_yaml_config_tst_LIBS = $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_yaml_config_tst

cpsw_strval_tst_SRCS      = cpsw_strval_tst.cc
cpsw_strval_tst_LIBS      = $(CPSW_LIBS)
TESTPROGRAMS              += cpsw_strval_tst

TEST_AXIV_YES=
TEST_AXIV_NO =cpsw_axiv_udp_tst
TEST_AXIV    =NO
DISABLED_TESTPROGRAMS=$(TEST_AXIV_$(TEST_AXIV))

PROGRAMS = udpsrv rssi_bridge
include $(CPSW_DIR)/rules.mak

# may override for individual test targets which will be
# executed with for all option sets, eg:
#
#    some_tst_run:RUN_OPTS='' '-a -b'
#
# (will run once w/o opts, a second time with -a -b)

# run for V2 and V1 and over TDEST demuxer (v2)
# NOTE: the t1 test will be slow as the packetized channel is scrambled
#       by udpsrv (and this increases average roundtrip time)
cpsw_netio_tst_RUN_OPTS = '-y cpsw_netio_tst_1.yaml'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_2.yaml -V1 -p8191'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_3.yaml -p8202 -r'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_4.yaml -p8203 -r -t1'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_5.yaml -p8190 -V3'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_6.yaml -p8189 -V3 -b'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_7.yaml -p8188 -V3 -r'
cpsw_netio_tst_RUN_OPTS+= '-y cpsw_netio_tst_8.yaml -p8204 -V3 -r -2 -t1'
cpsw_netio_tst_RUN_OPTS+= '-p8188 -V3 -r -R0'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_1.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_2.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_3.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_4.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_5.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_6.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_7.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_8.yaml'

cpsw_netio_tst_run:     RUN_OPTS=$(cpsw_netio_tst_RUN_OPTS)

cpsw_srpmux_tst_run:    RUN_OPTS='' '-V1 -p8191' '-p8202 -r' '-2 -p8204 -r'

cpsw_axiv_udp_tst_run:  RUN_OPTS='-y cpsw_axiv_udp_tst_1.yaml' '-Y cpsw_axiv_udp_tst_1.yaml' '-a192.168.2.10:8193:8194 -R -V3 -D0 -r -d1 -S100 -y cpsw_axiv_udp_tst_2.yaml' '-Y cpsw_axiv_udp_tst_2.yaml -S100'

cpsw_axiv_udp_tst_run:  RUN_VARS=PRBS_BASE=0x0a030000

cpsw_sval_tst_run:      RUN_OPTS='-y cpsw_sval_tst.yaml' '-Y cpsw_sval_tst.yaml'

# error percentage should be >  value used for udpsrv (-L) times number
# of fragments (-f)
cpsw_stream_tst_run:    RUN_OPTS='-e 22 -y cpsw_stream_tst_1.yaml' '-s8203 -R -y cpsw_stream_tst_2.yaml' '-s8204 -R -2 -y cpsw_stream_tst_3.yaml' '-e 22 -Y cpsw_stream_tst_1.yaml' '-Y cpsw_stream_tst_2.yaml' '-2 -Y cpsw_stream_tst_3.yaml'

cpsw_path_tst_run:      RUN_OPTS='' '-Y'

cpsw_srpv3_large_tst_run: RUN_OPTS='' '-V2' '-2'

cpsw_enum_tst_run:      RUN_OPTS='-y cpsw_enum_tst.yaml' '-Y cpsw_enum_tst.yaml' '-Y cpsw_enum_tst.yaml -q -C ""  >./cpsw_enum_tst_cfg.yaml' '-L ./cpsw_enum_tst_cfg.yaml'

cpsw_command_tst_run:   RUN_OPTS='-y cpsw_command_tst.yaml' '-Y cpsw_command_tst.yaml'

rssi_tst_run:           RUN_OPTS='-s500' '-n30000 -G2' '-n30000 -L1'

#cpsw_netio_tst: udpsrv
