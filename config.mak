# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

# In this file the desired target architectures
# as well as toolchain paths and prefixes are
# defined.
#
# The names for target architectures can be picked
# arbitrarily but should not characters besides
# [0-9][a-z][A-Z][.][-][_]

# By default the 'host' architecture is built. But
# you can redefine the name of the host architecture:
# HARCH = www
#HARCH=linux-x86_64
HARCH=rhel6-x86_64

# Assuming you also want 'xxx' and 'yyy' then you'd
# add these to the 'ARCHES' variable:

# ARCHES += xxx yyy

#ARCHES += linuxRT-x86_64 buildroot-2016.11.1-x86_64
ARCHES += buildroot-2016.11.1-x86_64 buildroot-2015.02-x86_64


# Next, you need to define prefixes (which may include
# absolute paths) so that e.g., $(CROSS_xxx)gcc can be
# used as a C compiler, e.g.:
BR2016_VERSION=2016.11.1
BR2015_VERSION=2015.02

BR_HARCH=linux-x86_64

BR_PATH=/afs/slac/package/linuxRT/buildroot-$(1)/host/$(BR_HARCH)/$(2)/usr/bin/$(2)-linux-

# CROSS_xxx = path_to_xxx_tools/xxx-
# CROSS_yyy = path_to_yyy_tools/yyy-
#
#CROSS_linux_x86_64  =# host build needs no prefix
#CROSS_linuxRT_x86_64=$(call BR_PATH,x86_64)
#CROSS_linuxRT_x86   =$(call BR_PATH,i686)
#CROSS_linuxRT_zynq  =$(call BR_PATH,arm)
CROSS_rhel6_x86_64   =#
CROSS_buildroot_2016_11_1_x86_64=$(call BR_PATH,$(BR2016_VERSION),x86_64)
CROSS_buildroot_2015_02_x86_64=$(call BR_PATH,$(BR2015_VERSION),x86_64)


# (you can override 'gcc' by another compiler by setting CC_<arch>)

# CPSW requires 'boost'. If this installed at a non-standard
# location (where it cannot be found automatically by the tools)
# then you need to set one of the variables
#
# boostinc_DIR_xxx,
# boostinc_DIR_default,
# boostinc_DIR,
# boost_DIR,
# boost_DIR_xxx,
# boost_DIR_default
#
# to the directory where boost headers are installed.
# In case of 'boost_DIR, boost_DIR_xxx, boost_DIR_default'
# a '/include' suffix is attached
# These aforementioned variables are tried in the listed
# order and the first one which is defined is used.
# Thus, e.g.,
#
# boostinc_DIR_xxx = /a/b/c
# boost_DIR_yyy     = /e/f
# boost_DIR_default =
#
# will use /a/b/c for architecture 'xxx' and /e/f/include for 'yyy'
# and nothing for any other one (including the 'host').
#
# Likewise, you must set
# boostlib_DIR_xxx, boostlib_DIR_default, boostlib_DIR
# (or any of boost_DIR, boost_DIR_xxx, boost_DIR_default in 
# which case a '/lib' suffix is attached) if 'boost' libraries
# cannot be found automatically by the (cross) tools.
#
# Note 'boost_DIR' (& friends) must be absolute paths
# or relative to $(CPSW_DIR).
#

# 
BOOST_VERSION=1.63.0
BOOST_PATH=$(PACKAGE_TOP)/boost/$(BOOST_VERSION)

#boost_DIR_linux_x86_64  =$(BOOST_PATH)/rhel6-x86_64
#boost_DIR_linuxRT_x86_64=$(BOOST_PATH)/buildroot-$(BR_VERSION)-x86_64
boost_DIR_rhel6_x86_64  =$(BOOST_PATH)/rhel6-x86_64
boost_DIR_buildroot_2016_11_1_x86_64=$(BOOST_PATH)/buildroot-$(BR2016_VERSION)-x86_64
boost_DIR_buildroot_2015_02_x86_64=$(BOOST_PATH)/buildroot-$(BR2015_VERSION)-x86_64

# YAML-CPP
#
# Analogous to the boost-specific variables a set of
# variables prefixed by 'yaml_cpp_' is used to identify
# the install location of yaml-cpp headers and library.
# 
YAML_CPP_VERSION         = yaml-cpp-0.5.3
YAML_CPP_PATH            = $(PACKAGE_TOP)/yaml-cpp/$(YAML_CPP_VERSION)

#yaml_cpp_DIR_linux_x86_64  =$(YAML_CPP_PATH)/rhel6-x86_64
#yaml_cpp_DIR_linuxRT_x86_64=$(YAML_CPP_PATH)/buildroot-$(BR_VERSION)-x86_64
yaml_cpp_DIR_rhel6_x86_64  =$(YAML_CPP_PATH)/rhel6-x86_64
yaml_cpp_DIR_buildroot_2016_11_1_x86_64=$(YAML_CPP_PATH)/buildroot-$(BR2016_VERSION)-x86_64
yaml_cpp_DIR_buildroot_2015_02_x86_64=$(YAML_CPP_PATH)/buildroot-$(BR2015_VERSION)-x86_64

# Whether to build static libraries (YES/NO)
WITH_STATIC_LIBRARIES_default=YES
# Whether to build shared libraries (YES/NO)
WITH_SHARED_LIBRARIES_default=YES

# Whether to build the python wrapper (this requires boost/python and python >= 2.7).
# The choice may be target specific, e.g.,
#
#WITH_PYCPSW_default   = NO
# WITH_PYCPSW_host    = YES
WITH_PYCPSW_rhel6_x86_64    = YES
#
# You may also have to set boost_DIR_<arch> or boostinc_DIR_<arch>
# and py_DIR_<arch> or pyinc_DIR_<arch> (see above)
py_DIR_rhel6_x86_64=/afs/slac/g/lcls/package/python/python2.7.9/rhel6-linux-x86_64/bin/
pyinc_DIR_rhel6_x86_64=/afs/slac/g/lcls/package/python/python2.7.9/rhel6-linux-x86_64/include/python2.7/

# Define an install location
INSTALL_DIR=../
