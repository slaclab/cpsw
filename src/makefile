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

POSTBUILD_SUBDIRS=test

HEADERS = cpsw_api_user.h cpsw_api_builder.h cpsw_api_timeout.h cpsw_error.h
DOCS = doc/README.yamlDefinition doc/README.yamlDefinition.html doc/README.configData.html doc/INSTALL.html

GENERATED_SRCS += git_version_string.h
GENERATED_SRCS += $(DOCS)

cpsw_SRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
cpsw_SRCS+= cpsw_entry_adapt.cc
cpsw_SRCS+= cpsw_stream_adapt.cc
cpsw_SRCS+= cpsw_sval.cc
cpsw_SRCS+= cpsw_const_sval.cc
cpsw_SRCS+= cpsw_command.cc
cpsw_SRCS+= cpsw_mmio_dev.cc
cpsw_SRCS+= cpsw_mem_dev.cc
cpsw_SRCS+= cpsw_null_dev.cc
cpsw_SRCS+= cpsw_netio_dev.cc
cpsw_SRCS+= cpsw_comm_addr.cc
cpsw_SRCS+= cpsw_srp_addr.cc
cpsw_SRCS+= cpsw_buf.cc
cpsw_SRCS+= cpsw_bufq.cc
cpsw_SRCS+= cpsw_event.cc
cpsw_SRCS+= cpsw_enum.cc
cpsw_SRCS+= cpsw_obj_cnt.cc
cpsw_SRCS+= cpsw_sock.cc
cpsw_SRCS+= cpsw_rssi_proto.cc
cpsw_SRCS+= cpsw_rssi.cc
cpsw_SRCS+= cpsw_rssi_states.cc
cpsw_SRCS+= cpsw_rssi_timer.cc
cpsw_SRCS+= cpsw_proto_mod.cc
cpsw_SRCS+= cpsw_proto_mod_depack.cc
cpsw_SRCS+= cpsw_proto_mod_udp.cc
cpsw_SRCS+= cpsw_proto_mod_tcp.cc
cpsw_SRCS+= cpsw_proto_mod_srpmux.cc
cpsw_SRCS+= cpsw_proto_mod_tdestmux.cc
cpsw_SRCS+= cpsw_proto_mod_rssi.cc
cpsw_SRCS+= cpsw_proto_stack_builder.cc
cpsw_SRCS+= cpsw_thread.cc
cpsw_SRCS+= cpsw_yaml.cc
cpsw_SRCS+= cpsw_preproc.cc
cpsw_SRCS+= cpsw_version.cc

DEP_HEADERS  = $(HEADERS)
DEP_HEADERS += cpsw_address.h
DEP_HEADERS += cpsw_buf.h
DEP_HEADERS += cpsw_event.h
DEP_HEADERS += cpsw_entry_adapt.h
DEP_HEADERS += cpsw_stream_adapt.h
DEP_HEADERS += cpsw_entry.h
DEP_HEADERS += cpsw_enum.h
DEP_HEADERS += cpsw_freelist.h
DEP_HEADERS += cpsw_hub.h
DEP_HEADERS += cpsw_mem_dev.h
DEP_HEADERS += cpsw_null_dev.h
DEP_HEADERS += cpsw_mmio_dev.h
DEP_HEADERS += cpsw_netio_dev.h
DEP_HEADERS += cpsw_srp_addr.h
DEP_HEADERS += cpsw_obj_cnt.h
DEP_HEADERS += cpsw_path.h
DEP_HEADERS += cpsw_sock.h
DEP_HEADERS += cpsw_proto_mod_depack.h
DEP_HEADERS += cpsw_proto_mod.h
DEP_HEADERS += cpsw_proto_mod_bytemux.h
DEP_HEADERS += cpsw_proto_mod_srpmux.h
DEP_HEADERS += cpsw_proto_mod_tdestmux.h
DEP_HEADERS += cpsw_proto_mod_udp.h
DEP_HEADERS += cpsw_proto_mod_tcp.h
DEP_HEADERS += cpsw_rssi_proto.h
DEP_HEADERS += cpsw_rssi.h
DEP_HEADERS += cpsw_rssi_timer.h
DEP_HEADERS += cpsw_proto_mod_rssi.h
DEP_HEADERS += cpsw_shared_obj.h
DEP_HEADERS += cpsw_sval.h
DEP_HEADERS += cpsw_const_sval.h
DEP_HEADERS += cpsw_thread.h
DEP_HEADERS += cpsw_mutex.h
DEP_HEADERS += cpsw_command.h
DEP_HEADERS += cpsw_yaml.h
DEP_HEADERS += cpsw_preproc.h

STATIC_LIBRARIES_YES+=cpsw
SHARED_LIBRARIES_YES+=cpsw

cpsw_yaml_xpand_SRCS += cpsw_yaml_xpand.cc
cpsw_yaml_xpand_LIBS += $(CPSW_LIBS)

cpsw_ypp_SRCS        += cpsw_ypp.cc cpsw_preproc.cc

# Python wrapper; only built if WITH_PYCPSW is set to YES (can be target specific)
pycpsw_so_SRCS      = cpsw_python.cc
pycpsw_so_LIBS      = $(BOOST_PYTHON_LIB) $(CPSW_LIBS)
pycpsw_so_CPPFLAGS  = $(addprefix -I,$(pyinc_DIR))
pycpsw_so_CXXFLAGS  = -fno-strict-aliasing

PYCPSW_YES          = pycpsw.so
PYCPSW              = $(PYCPSW_$(WITH_PYCPSW))

yaml_cpp_so_SRCS    = yaml_cpp_python.cc
yaml_cpp_so_LIBS    = $(BOOST_PYTHON_LIB) $(filter yaml-cpp%,$(CPSW_LIBS))
yaml_cpp_so_CPPFLAGS= $(addprefix -I,$(pyinc_DIR))
yaml_cpp_so_CXXFLAGS= -fno-strict-aliasing


PYYAML_YES          = yaml_cpp.so
PYYAML              = $(PYYAML_$(WITH_PYCPSW))

PROGRAMS         += cpsw_yaml_xpand cpsw_ypp $(PYCPSW) $(PYYAML)

include $(CPSW_DIR)/rules.mak

doc/README.yamlDefinition:doc/README.yamlDefinition.in
	awk 'BEGIN{ FS="[ \t\"]+" } /\<YAML_KEY_/{ printf("s/%s/%s/g\n", $$2, $$3); }' cpsw_yaml_keydefs.h | sed -f - $< > $@

%.html: %
	sed -e 's/&/\&amp/g' -e 's/</\&lt/g' -e 's/>/\&gt/g' $< | sed -e '/THECONTENT/{r /dev/stdin' -e 'd}' tmpl.html > $@


