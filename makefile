# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

CPSW_DIR=.

include $(CPSW_DIR)/defs.mak

HEADERS = cpsw_api_user.h cpsw_api_builder.h cpsw_api_timeout.h cpsw_error.h

GENERATED_SRCS += git_version_string.h
GENERATED_SRCS += README.yamlDefinition
GENERATED_SRCS += README.yamlDefinition.html
GENERATED_SRCS += README.configData.html
GENERATED_SRCS += INSTALL.html

cpsw_SRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
cpsw_SRCS+= cpsw_entry_adapt.cc
cpsw_SRCS+= cpsw_sval.cc
cpsw_SRCS+= cpsw_command.cc
cpsw_SRCS+= cpsw_mmio_dev.cc
cpsw_SRCS+= cpsw_mem_dev.cc
cpsw_SRCS+= cpsw_netio_dev.cc
cpsw_SRCS+= cpsw_buf.cc
cpsw_SRCS+= cpsw_bufq.cc
cpsw_SRCS+= cpsw_event.cc
cpsw_SRCS+= cpsw_enum.cc
cpsw_SRCS+= cpsw_obj_cnt.cc
cpsw_SRCS+= cpsw_rssi_proto.cc
cpsw_SRCS+= cpsw_rssi.cc
cpsw_SRCS+= cpsw_rssi_states.cc
cpsw_SRCS+= cpsw_rssi_timer.cc
cpsw_SRCS+= cpsw_proto_mod.cc
cpsw_SRCS+= cpsw_proto_mod_depack.cc
cpsw_SRCS+= cpsw_proto_mod_udp.cc
cpsw_SRCS+= cpsw_proto_mod_srpmux.cc
cpsw_SRCS+= cpsw_proto_mod_tdestmux.cc
cpsw_SRCS+= cpsw_proto_mod_rssi.cc
cpsw_SRCS+= cpsw_thread.cc
cpsw_SRCS+= cpsw_yaml.cc
cpsw_SRCS+= cpsw_preproc.cc

DEP_HEADERS  = $(HEADERS)
DEP_HEADERS += cpsw_address.h
DEP_HEADERS += cpsw_buf.h
DEP_HEADERS += cpsw_event.h
DEP_HEADERS += cpsw_entry_adapt.h
DEP_HEADERS += cpsw_entry.h
DEP_HEADERS += cpsw_enum.h
DEP_HEADERS += cpsw_freelist.h
DEP_HEADERS += cpsw_hub.h
DEP_HEADERS += cpsw_mem_dev.h
DEP_HEADERS += cpsw_mmio_dev.h
DEP_HEADERS += cpsw_netio_dev.h
DEP_HEADERS += cpsw_obj_cnt.h
DEP_HEADERS += cpsw_path.h
DEP_HEADERS += cpsw_proto_mod_depack.h
DEP_HEADERS += cpsw_proto_mod.h
DEP_HEADERS += cpsw_proto_mod_bytemux.h
DEP_HEADERS += cpsw_proto_mod_srpmux.h
DEP_HEADERS += cpsw_proto_mod_tdestmux.h
DEP_HEADERS += cpsw_proto_mod_udp.h
DEP_HEADERS += cpsw_rssi_proto.h
DEP_HEADERS += cpsw_rssi.h
DEP_HEADERS += cpsw_rssi_timer.h
DEP_HEADERS += cpsw_proto_mod_rssi.h
DEP_HEADERS += cpsw_shared_obj.h
DEP_HEADERS += cpsw_sval.h
DEP_HEADERS += cpsw_thread.h
DEP_HEADERS += cpsw_mutex.h
DEP_HEADERS += cpsw_command.h
DEP_HEADERS += cpsw_yaml.h
DEP_HEADERS += cpsw_preproc.h
DEP_HEADERS += crc32-le-tbl-4.h
DEP_HEADERS += udpsrv_regdefs.h
DEP_HEADERS += udpsrv_port.h
DEP_HEADERS += udpsrv_rssi_port.h
DEP_HEADERS += udpsrv_util.h

STATIC_LIBRARIES_YES+=cpsw
SHARED_LIBRARIES_YES+=cpsw

tstaux_SRCS+= crc32-le-tbl-4.c

STATIC_LIBRARIES_YES+=tstaux
SHARED_LIBRARIES_YES+=tstaux

STATIC_LIBRARIES=$(STATIC_LIBRARIES_$(WITH_STATIC_LIBRARIES))
SHARED_LIBRARIES=$(SHARED_LIBRARIES_$(WITH_SHARED_LIBRARIES))

udpsrv_SRCS = udpsrv.c
udpsrv_SRCS+= udpsrv_port.cc
udpsrv_SRCS+= udpsrv_util.cc
udpsrv_SRCS+= udpsrv_mod_mem.cc
udpsrv_SRCS+= udpsrv_mod_axiprom.cc

udpsrv_CXXFLAGS+= -DUDPSRV

udpsrv_LIBS = tstaux $(CPSW_LIBS)

cpsw_yaml_xpand_SRCS += cpsw_yaml_xpand.cc
cpsw_yaml_xpand_LIBS += $(CPSW_LIBS)

# Python wrapper; only built if WITH_PYCPSW is set to YES (can be target specific)
pycpsw_so_SRCS    = cpsw_python.cc
pycpsw_so_LIBS    = $(BOOST_PYTHON_LIB) $(CPSW_LIBS)
pycpsw_so_CPPFLAGS= $(addprefix -I,$(pyinc_DIR))

PYCPSW_YES        = pycpsw.so
PYCPSW            = $(PYCPSW_$(WITH_PYCPSW))

PROGRAMS                += udpsrv cpsw_yaml_xpand $(PYCPSW)

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

rssi_tst_SRCS            = rssi_tst.cc udpsrv_port.cc udpsrv_util.cc
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

TEST_AXIV_YES=
TEST_AXIV_NO =cpsw_axiv_udp_tst
TEST_AXIV    =NO
DISABLED_TESTPROGRAMS=$(TEST_AXIV_$(TEST_AXIV))

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
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_1.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_2.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_3.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_4.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_5.yaml'
cpsw_netio_tst_RUN_OPTS+= '-Y cpsw_netio_tst_6.yaml'

cpsw_netio_tst_run:     RUN_OPTS=$(cpsw_netio_tst_RUN_OPTS)

cpsw_srpmux_tst_run:    RUN_OPTS='' '-V1 -p8191' '-p8202 -r'

cpsw_axiv_udp_tst_run:  RUN_OPTS='-y cpsw_axiv_udp_tst_1.yaml' '-Y cpsw_axiv_udp_tst_1.yaml' '-a192.168.2.10:8193:8194 -R -V3 -D0 -r -d1 -S100 -y cpsw_axiv_udp_tst_2.yaml' '-Y cpsw_axiv_udp_tst_2.yaml -S100'

cpsw_axiv_udp_tst_run:  RUN_VARS=PRBS_BASE=0x0a030000

cpsw_sval_tst_run:      RUN_OPTS='-y cpsw_sval_tst.yaml' '-Y cpsw_sval_tst.yaml'

# error percentage should be >  value used for udpsrv (-L) times number
# of fragments (-f)
cpsw_stream_tst_run:    RUN_OPTS='-e 22 -y cpsw_stream_tst_1.yaml' '-s8203 -R -y cpsw_stream_tst_2.yaml' '-Y cpsw_stream_tst_1.yaml' '-Y cpsw_stream_tst_2.yaml'

cpsw_path_tst_run:      RUN_OPTS='' '-Y'

cpsw_enum_tst_run:      RUN_OPTS='-y cpsw_enum_tst.yaml' '-Y cpsw_enum_tst.yaml'

cpsw_command_tst_run:   RUN_OPTS='-y cpsw_command_tst.yaml' '-Y cpsw_command_tst.yaml'

rssi_tst_run:           RUN_OPTS='-s500' '-n30000 -G2' '-n30000 -L1'

cpsw_netio_tst: udpsrv

README.yamlDefinition:README.yamlDefinition.in
	awk 'BEGIN{ FS="[ \t\"]+" } /\<YAML_KEY_/{ printf("s/%s/%s/g\n", $$2, $$3); }' cpsw_yaml_keydefs.h | sed -f - $< > $@

%.html: %
	sed -e 's/&/\&amp/g' -e 's/</\&lt/g' -e 's/>/\&gt/g' $< | sed -e '/THECONTENT/{r /dev/stdin' -e 'd}' tmpl.html > $@
