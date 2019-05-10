# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

# (Default) Definitions for CPSW Makefiles

# host architecture
HARCH_DEFAULT:=linux-$(shell uname -m)
HARCH=$(HARCH_DEFAULT)
# Architectures to build
ARCHES=$(HARCH)

# Save default (host) tools
CC_host    :=$(CC)
CXX_host   :=$(CXX)
AR_host    :=$(AR)
RANLIB_host:=$(or $(RANLIB),ranlib)

GIT_VERSION_STRING:=$(shell echo -n '"'`git describe --always --dirty`'"')

# arch2var(VARIABLE_PREFIX)
define arch2var
$(1)=$$(or $$($(1)_$$(TARNM)),$$($(1)_default))
endef

# arch2libvar(VARIABLE_PREFIX)
define arch2libvar
$(1)_DIR=$$(or $$($(1)_DIR_$$(TARNM)),$$($(1)_DIR_default))
$(1)inc_DIR=$$(or $$($(1)inc_DIR_$$(TARNM)),$$($(1)inc_DIR_default),$$(addsuffix /include,$$($(1)_DIR)))
$(1)lib_DIR=$$(or $$($(1)lib_DIR_$$(TARNM)),$$($(1)lib_DIR_default),$$(addsuffix /lib,$$($(1)_DIR)))
endef

# When expanding any of the ARCHSPECIFIC_VARS
# we'll try $(var_$(TARNM)) and $(var_default)
ARCHSPECIFIC_VARS+=CROSS
ARCHSPECIFIC_VARS+=WITH_SHARED_LIBRARIES
ARCHSPECIFIC_VARS+=WITH_STATIC_LIBRARIES
ARCHSPECIFIC_VARS+=WITH_PYCPSW
ARCHSPECIFIC_VARS+=WITH_BOOST
ARCHSPECIFIC_VARS+=BOOST_PYTHON_LIB
ARCHSPECIFIC_VARS+=USR_CPPFLAGS
ARCHSPECIFIC_VARS+=USR_CXXFLAGS
ARCHSPECIFIC_VARS+=USR_CFLAGS
ARCHSPECIFIC_VARS+=USR_LDFLAGS
ARCHSPECIFIC_VARS+=LOC_CPPFLAGS
ARCHSPECIFIC_VARS+=LOC_CXXFLAGS
ARCHSPECIFIC_VARS+=LOC_CFLAGS
ARCHSPECIFIC_VARS+=LOC_LDFLAGS
ARCHSPECIFIC_VARS+=USE_CXX11
ARCHSPECIFIC_VARS+=CYTHON

# For all of the ARCHSPECIFIC_LIBVARS
# we'll construct
# var_DIR, varinc_DIR, varlib_DIR
# and let each one try e.g.,
#
# $(<var>inc_DIR_$(TARNM)), $(<var>inc_DIR_default), $(<var>_DIR)/include
#
# (with $(<var>_DIR) also trying $(<var>_DIR_$(TARNM)), $(<var>_DIR_default))
#
ARCHSPECIFIC_LIBVARS+=yaml_cpp
ARCHSPECIFIC_LIBVARS+=boost
ARCHSPECIFIC_LIBVARS+=py

$(foreach var,$(ARCHSPECIFIC_VARS),$(eval $(call arch2var,$(var))))

$(foreach var,$(ARCHSPECIFIC_LIBVARS),$(eval $(call arch2libvar,$(var))))

CROSS_default=$(addsuffix -,$(filter-out $(HARCH),$(TARCH)))

# Tools
CC     =$(CROSS)$(or $(CC_$(TARNM)),$(CC_default),gcc)
CXX    =$(CROSS)$(or $(CXX_$(TARNM)),$(CXX_default),g++)
AR     =$(CROSS)$(or $(AR_$(TARNM)),$(AR_default),ar)
RANLIB =$(CROSS)$(or $(RANLIB_$(TARNM)),$(RANLIB_default),ranlib)
INSTALL=install -C

CYTHON = cython

# Tool options
OPT_CXXFLAGS=-g -Wall -O2
OPT_CFLAGS  =-g -Wall -O2

STD_CXXFLAGS_USE_CXX11_YES+=-std=c++11

STD_CXXFLAGS+=$(STD_CXXFLAGS_USE_CXX11_$(USE_CXX11))

BOOST_CPPFLAGS_NO+=-DWITHOUT_BOOST

CPPFLAGS+= $(addprefix -I,$(SRCDIR) $(INCLUDE_DIRS) $(INSTALL_DIR:%=%/include))
CPPFLAGS+= $(addprefix -I,$(subst :, ,$(cpswinc_DIRS)))
CPPFLAGS+= $(USR_CPPFLAGS)
CPPFLAGS+= $(LOC_CPPFLAGS)
CPPFLAGS+= $(BOOST_CPPFLAGS_$(WITH_BOOST))
CXXFLAGS+= $(OPT_CXXFLAGS)
CXXFLAGS+= $(STD_CXXFLAGS)
CXXFLAGS+= $(USR_CXXFLAGS)
CXXFLAGS+= $(LOC_CXXFLAGS)
CFLAGS  += $(OPT_CFLAGS)
CFLAGS  += $(USR_CFLAGS)
CFLAGS  += $(LOC_CFLAGS)

LDFLAGS += $(USR_LDFLAGS)
LDFLAGS += $(LOC_LDFLAGS)

VPATH=$(SRCDIR)

FILTERED_TBINS=$(filter-out $(DISABLED_TESTPROGRAMS) %.so,$(TESTPROGRAMS))

RUN_OPTS=''

# Libraries currently required by CPSW itself (and thus by anything using it)

# colon separated dirlist
# Note: += adds a whitespace
cpswinc_DIRS=$(CPSW_DIR)$(addprefix :,$(boostinc_DIR))$(addprefix :,$(yaml_cppinc_DIR))
# colon separated dirlist
cpsw_deplib_DIRS=$(addprefix :,$(boostlib_DIR))$(addprefix :,$(yaml_cpplib_DIR))
cpswlib_DIRS=$(addsuffix /O.$(TARCH),$(CPSW_DIR))$(cpsw_deplib_DIRS)

# Libraries CPSW requires -- must be added to application's <prog>_LIBS variable
CPSW_LIBS   = cpsw yaml-cpp pthread rt dl

STATIC_LIBRARIES=$(STATIC_LIBRARIES_$(WITH_STATIC_LIBRARIES))
SHARED_LIBRARIES=$(SHARED_LIBRARIES_$(WITH_SHARED_LIBRARIES))

# Default values -- DO NOT OVERRIDE HERE but in config.mak or config.local.mak
BOOST_PYTHON_LIB_default     =boost_python
WITH_SHARED_LIBRARIES_default=YES
WITH_STATIC_LIBRARIES_default=NO
WITH_PYCPSW_default          =$(or $(and $(pyinc_DIR),$(WITH_SHARED_LIBRARIES),CYTHON),NO)
WITH_BOOST_default           =YES

COMMA__:=,
SPACE__:=
SPACE__+=

ifndef SRCDIR
SRCDIR=.
endif

INSTALL_DOC=$(INSTALL_DIR)/$(TARCH)/doc
INSTALL_INC=$(INSTALL_DIR)/$(TARCH)/include
INSTALL_LIB=$(INSTALL_DIR)/$(TARCH)/lib
INSTALL_BIN=$(INSTALL_DIR)/$(TARCH)/bin

SHPTR_DIR_USE_CXX11_YES=std/
SHPTR_DIR_USE_CXX11_NO=boost/
SHPTR_DIR_USE_CXX11=$(SHPTR_DIR_USE_CXX11_$(USE_CXX11))

INCLUDE_DIRS+=$(CPSW_DIR)/$(SHPTR_DIR_USE_CXX11)

# CYTHON
# We don't want users to *have* cython installed. Also, ATM most cython versions
# found in distros do not support python 3.7.
# We check whether we really need to run cython by computing a checksum over the
# dependencies of a cython-generated '.cc/.c' file. The checksum is also stored
# in a file. Both, the cython-generated source *and* the file holding the checksum
# are maintained in git. If the '.cc' file is outdated then the rule to remamke
# it can execute $(call maybe_run_cython, CMD). The macro compares the checksum
# with the stored checksum and if they match the '.cc' is merely 'touch'ed in
# order to update its timestamp. Only if a mismatch is found is cython re-run
# and the checksum-file updated.
#
# ARGUMENT: CYTHON command; should generate $@, e.g.,
#
#           $(call maybe_run_cython,$(CYTHON) $< -o $@)
#
define maybe_run_cython
	@echo "Check if we have to remake cython-generated sources"
	cython_cksum="`cat $^ | cksum`" ; \
    stamp_file="$(addsuffix /,$(dirname $@))$(basename $@).stamp" ; \
	if [ -e "$@" -a -e "$${stamp_file}" ] && [ "$${cython_cksum}" = "`cat $${stamp_file}`" ] ; then\
		echo "Checksum found and it matches 'stamp'; just touching $@" ; \
		touch $@; \
	else \
		echo "Remaking cython sources" ; \
		if $(1) ; then \
			echo "Updating '$${stamp_file}'"; \
			echo "$${cython_cksum}" > "$${stamp_file}"; \
		else \
			echo "CYTHON command failed" ; \
			false ; \
		fi \
	fi
endef

# definitions
include $(CPSW_DIR)/../config.mak
-include $(CPSW_DIR)/../config.local.mak
-include $(SRCDIR)/local.mak

ifndef TOPDIR
# Heuristics to find a top-level makefile; assume cpsw can be
# in a sub-directory...
TOPDIR:=$(shell NP=./; while P=$${NP} && NP=$${NP}../ && [ -e $${NP}makefile ] && grep -q CPSW_DIR $${NP}makefile ; do true; done; echo $${P})
endif
