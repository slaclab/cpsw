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

# API version is the major version number...
CPSW_API_VERSION:=$(shell echo '$(GIT_VERSION_STRING)' | sed -n -e 's/[ "]*R\([0-9]\+\).*/\1/p')

# escape '#' and '$' for make
CURRENT_API_PATTERN=\([ \t]*\#define[ \t]\+CPSW_API_VERSION\([ \t]\+\|$$\)\)\([0-9]\+\)\?\(.*$$\)

POSTBUILD_SUBDIRS+=libTstAux
POSTBUILD_SUBDIRS+=rssi_bridge
POSTBUILD_SUBDIRS+=test

HEADERS        += cpsw_api_user.h cpsw_api_builder.h cpsw_api_timeout.h cpsw_error.h cpsw_stdio.h $(SHPTR_DIR_USE_CXX11)cpsw_shared_ptr.h
DOCS           += doc/README.yamlDefinition.md doc/README.yamlDefinition.html doc/README.configData.html doc/INSTALL.html doc/README.html

GENERATED_SRCS += cpsw_git_version_string.h
GENERATED_SRCS += $(DOCS)

VPATH+=:$(SRCDIR)/socks/
VPATH+=:$(SRCDIR)/rssi_bridge/

cpsw_SRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
cpsw_SRCS+= cpsw_api_builder.cc
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
cpsw_SRCS+= cpsw_srp_transactions.cc
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
cpsw_SRCS+= cpsw_proto_depack.cc
cpsw_SRCS+= cpsw_proto_mod_depack.cc
cpsw_SRCS+= cpsw_proto_mod_udp.cc
cpsw_SRCS+= cpsw_proto_mod_tcp.cc
cpsw_SRCS+= cpsw_proto_mod_srpmux.cc
cpsw_SRCS+= cpsw_proto_mod_tdestmux.cc
cpsw_SRCS+= cpsw_proto_mod_tdestmux2.cc
cpsw_SRCS+= cpsw_proto_mod_rssi.cc
cpsw_SRCS+= cpsw_proto_stack_builder.cc
cpsw_SRCS+= cpsw_thread.cc
cpsw_SRCS+= cpsw_yaml.cc
cpsw_SRCS+= cpsw_preproc.cc
cpsw_SRCS+= cpsw_version.cc
cpsw_SRCS+= cpsw_debug.cc
cpsw_SRCS+= cpsw_async_io.cc
cpsw_SRCS+= cpsw_crc32_le.cc
cpsw_SRCS+= libSocksConnect.c
cpsw_SRCS+= libSocksNegotiate4.c
cpsw_SRCS+= libSocksNegotiate5.c
cpsw_SRCS+= libSocksParseName.c
cpsw_SRCS+= libSocksGetByName.c
cpsw_SRCS+= libSocksUtil.c
cpsw_SRCS+= rpcMapLookup.c
cpsw_SRCS+= prot_xdr.c
cpsw_SRCS+= cpsw_fs_addr.cc
cpsw_SRCS+= cpsw_yaml_merge.cc
cpsw_SRCS+= cpsw_flookup.cc
cpsw_SRCS+= cpsw_stdio.cc

DEP_HEADERS  = $(HEADERS)
DEP_HEADERS += cpsw_address.h
DEP_HEADERS += cpsw_buf.h
DEP_HEADERS += cpsw_event.h
DEP_HEADERS += cpsw_entry_adapt.h
DEP_HEADERS += cpsw_stream_adapt.h
DEP_HEADERS += cpsw_entry.h
DEP_HEADERS += cpsw_enum.h
DEP_HEADERS += cpsw_freelist.h
DEP_HEADERS += cpsw_freelist_stack.h
DEP_HEADERS += cpsw_queue.h
DEP_HEADERS += cpsw_hub.h
DEP_HEADERS += cpsw_mem_dev.h
DEP_HEADERS += cpsw_null_dev.h
DEP_HEADERS += cpsw_mmio_dev.h
DEP_HEADERS += cpsw_netio_dev.h
DEP_HEADERS += cpsw_srp_addr.h
DEP_HEADERS += cpsw_srp_transactions.h
DEP_HEADERS += cpsw_obj_cnt.h
DEP_HEADERS += cpsw_path.h
DEP_HEADERS += cpsw_sock.h
DEP_HEADERS += cpsw_proto_depack.h
DEP_HEADERS += cpsw_proto_mod_depack.h
DEP_HEADERS += cpsw_proto_mod.h
DEP_HEADERS += cpsw_proto_mod_bytemux.h
DEP_HEADERS += cpsw_proto_mod_srpmux.h
DEP_HEADERS += cpsw_proto_mod_tdestmux.h
DEP_HEADERS += cpsw_proto_mod_tdestmux2.h
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
DEP_HEADERS += cpsw_debug.h
DEP_HEADERS += cpsw_async_io.h
DEP_HEADERS += libSocks.h
DEP_HEADERS += libSocksUtil.h
DEP_HEADERS += cpsw_swig_python.h
DEP_HEADERS += cpsw_python.h
DEP_HEADERS += cpsw_cython.h
DEP_HEADERS += cpsw_aligned_mmio.h
DEP_HEADERS += cpsw_fs_addr.h
DEP_HEADERS += cpsw_yaml_merge.h
DEP_HEADERS += cpsw_flookup.h
DEP_HEADERS += cpsw_stdio.h

STATIC_LIBRARIES_YES+=cpsw
SHARED_LIBRARIES_YES+=cpsw

cpsw_yaml_xpand_SRCS += cpsw_yaml_xpand.cc
cpsw_yaml_xpand_LIBS += $(CPSW_LIBS)

cpsw_ypp_SRCS        += cpsw_ypp.cc
cpsw_ypp_LIBS_YES    += $(CPSW_STATIC_LIBS)
cpsw_ypp_LIBS_NO     += $(CPSW_LIBS)
cpsw_ypp_LIBS        += $(cpsw_ypp_LIBS_$(WITH_STATIC_LIBRARIES))

cpsw_yaml_merge_SRCS += cpsw_yaml_merge.cc cpsw_yaml_merge_main.cc cpsw_stdio.cc
cpsw_yaml_merge_LIBS += yaml-cpp


# Python wrapper; only built if WITH_PYCPSW is set to CYTHON/BOOST/SWIG
# (can be target specific)
pycpsw_so_SRCS_BOOST  = cpsw_boost_python.cc cpsw_python.cc
pycpsw_so_SRCS_CYTHON = cpsw_python.cc pycpsw.cc cpsw_cython.cc
pycpsw_so_LIBS_BOOST  = $(BOOST_PYTHON_LIB) $(CPSW_LIBS)
pycpsw_so_LIBS_CYTHON = $(CPSW_LIBS)
pycpsw_so_CPPFLAGS    = $(addprefix -I,$(pyinc_DIR)) -I.
pycpsw_so_CXXFLAGS    = -fno-strict-aliasing

pycpsw_so_SRCS        = $(pycpsw_so_SRCS_$(WITH_PYCPSW))
pycpsw_so_LIBS        = $(pycpsw_so_LIBS_$(WITH_PYCPSW))

PYCPSW_BOOST          = pycpsw.so
PYCPSW_CYTHON         = pycpsw.so
PYCPSW_SWIG           = _pycpsw.so
PYCPSW                = $(PYCPSW_$(WITH_PYCPSW))

yaml_cpp_so_SRCS      = $(yaml_cpp_so_SRCS_$(WITH_PYCPSW))
yaml_cpp_so_LIBS      = $(yaml_cpp_so_LIBS_$(WITH_PYCPSW))

yaml_cpp_so_SRCS_BOOST= yaml_cpp_python.cc
yaml_cpp_so_SRCS_CYTHON= pyyaml_cpp.cc
yaml_cpp_so_LIBS_BOOST= $(BOOST_PYTHON_LIB) $(filter yaml-cpp%,$(CPSW_LIBS))
yaml_cpp_so_LIBS_CYTHON= $(filter yaml-cpp%,$(CPSW_LIBS))
yaml_cpp_so_CPPFLAGS  = $(addprefix -I,$(pyinc_DIR))
yaml_cpp_so_CXXFLAGS  = -fno-strict-aliasing

_pycpsw_so_SRCS    += pycpsw_wrap.cc
_pycpsw_so_SRCS    += cpsw_swig_python.cc
_pycpsw_so_SRCS    += cpsw_python.cc
_pycpsw_so_CPPFLAGS+= $(addprefix -I,$(pyinc_DIR))
_pycpsw_so_LIBS    += $(CPSW_LIBS)

PYYAML_BOOST        = yaml_cpp.so
PYYAML_CYTHON       = yaml_cpp.so
PYYAML              = $(PYYAML_$(WITH_PYCPSW))

PROGRAMS         += cpsw_yaml_xpand cpsw_ypp $(PYCPSW) $(PYYAML) cpsw_yaml_merge

include $(CPSW_DIR)/rules.mak

# Note that -builtin led to the cyclic-GC losing
# cycles that point to swig-wrapped objects.
# No problem without -builting

pycpsw_wrap.cc: pycpsw.i cpsw_api_user.h cpsw_swig_python.h
	swig -python -c++ -o $@ $<

$(addsuffix /,$(SRCDIR))pycpsw.cc: pycpsw.pyx pycpsw.pxd yaml_cpp.pxd cpsw_api_user.h
	$(call maybe_run_cython,$(CYTHON) --cplus $(addprefix -I,$(yaml_cppinc_DIR)) -X language_level=3 -X embedsignature=True $< -o $@)

$(addsuffix /,$(SRCDIR))pyyaml_cpp.cc: yaml_cpp.pyx yaml_cpp.pxd
	$(call maybe_run_cython,$(CYTHON) --cplus $(addprefix -I,$(yaml_cppinc_DIR)) -X language_level=3 -X embedsignature=True $< -o $@)

doc/README.yamlDefinition.md:doc/README.yamlDefinition.in
	awk 'BEGIN{ FS="[ \t\"]+" } /\<YAML_KEY_/{ printf("s/%s/%s/g\n", $$2, $$3); }' cpsw_yaml_keydefs.h | sed -f - $< > $@
	if grep 'YAML_KEY_' $@; then \
		echo 'ERROR: Some YAML_KEY_ tags not expanded (ill-spelled?)' ; \
		$(RM) $@ ; \
		exit 1; \
	fi

%.html: %.md
	sed -e 's/&/\&amp/g' -e 's/</\&lt/g' -e 's/>/\&gt/g' $< | sed -e '/THECONTENT/{r /dev/stdin' -e 'd}' tmpl.html > $@

../cpsw_api_user.h: FORCE
	@CURRENT_API_VERSION=`sed -n -e 's/$(CURRENT_API_PATTERN)/\3/p' $@`; \
	if  [ 0`sed -n -e 's/$(CURRENT_API_PATTERN)/\3/p' $@` -ne 0$(CPSW_API_VERSION) ] ; then \
		sed -i -e 's/$(CURRENT_API_PATTERN)/\1$(CPSW_API_VERSION)\4/' $@ ; \
		echo "Current: $${CURRENT_API_VERSION} GIT: $(CPSW_API_VERSION)" ; \
		echo "API MISMATCH; fixing $@" ; \
	fi

.PHONY: foo
foo:
	echo $(CPSW_API_VERSION)
