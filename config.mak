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

PACKAGE_TOP=/afs/slac/g/lcls/package

# By default the 'host' architecture is built. But
# you can redefine the name of the host architecture:
# HARCH = www
#HARCH=linux-x86_64
HARCH=rhel6-x86_64

# Assuming you also want 'xxx' and 'yyy' then you'd
# add these to the 'ARCHES' variable:

# ARCHES += xxx yyy

ARCHES += buildroot-2019.08-x86_64 buildroot-2019.08-i686 buildroot-2019.08-arm
ARCHES += buildroot-2016.11.1-x86_64 buildroot-2016.11.1-i686 buildroot-2016.11.1-arm

# Next, you need to define prefixes (which may include
# absolute paths) so that e.g., $(CROSS_xxx)gcc can be
# used as a C compiler e.g.,
#
# CROSS_xxx = path_to_xxx_tools/xxx-
# CROSS_yyy = path_to_yyy_tools/yyy-
#
# (you can override 'gcc' by another compiler by setting CC_<arch>)

# definitions for multiple buildroot targets

BR_HARCH=linux-x86_64

# extract version and target
BR_VERSN=$(word 2,$(subst -, ,$(TARCH)))
# map zynq->arm for buildroot
BR_TARCH=$(subst zynq,arm,$(word 3,$(subst -, ,$(TARCH))))

# cross compiler path
BR_CROSS=/afs/slac/package/linuxRT/buildroot-$(BR_VERSN)/host/$(BR_HARCH)/$(BR_TARCH)/usr/bin/$(BR_TARCH)-linux-

# map target arch-name into a make variable name
BR_ARNAMS=$(subst .,_,$(subst -,_,$(filter buildroot-%,$(ARCHES))))

# for all buildroot targets create
# CROSS_buildroot_<version>_<target>
$(foreach brarnam,$(BR_ARNAMS),$(eval CROSS_$(brarnam)=$$(BR_CROSS)))

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
BOOST_VERSION=1.64.0
BOOST_PATH=$(PACKAGE_TOP)/boost/$(BOOST_VERSION)

boost_DIR_default=$(BOOST_PATH)/$(TARCH)

# YAML-CPP
#
# Analogous to the boost-specific variables a set of
# variables prefixed by 'yaml_cpp_' is used to identify
# the install location of yaml-cpp headers and library.
#
YAML_CPP_VERSION         = yaml-cpp-0.5.3_boost-1.64.0
YAML_CPP_PATH            = $(PACKAGE_TOP)/yaml-cpp/$(YAML_CPP_VERSION)

yaml_cpp_DIR_default=$(YAML_CPP_PATH)/$(TARCH)

# Whether to build static libraries (YES/NO)
WITH_STATIC_LIBRARIES_default=YES
# Whether to build shared libraries (YES/NO)
WITH_SHARED_LIBRARIES_default=YES

# Whether to build the python wrapper (this requires boost/python and python >= 2.7).
# The choice may be target specific in the usual way
#
# build python if the 'py_DIR' variable is defined and
# pointing to an existing 'libboost_python'
#
# The python wrapper can be built with boost_python, SWIG or cython.
# Set WITH_PYCPSW to BOOST, SWIG or CYTHON. Note that the SWIG
# port is incomplete and does not support asyncronous getVal() ATM.
#
FOUND_BOOST_PY=$(if $(boostlib_DIR),$(if $(wildcard $(boostlib_DIR)/libboost_python*),YES,NO),YES)

#Default defined in 'defs.mak':
#  use CYTHON if pyinc_DIR is defined and shared libraries
#  are enabled; otherwise don't build PYCPSW.
#WITH_PYCPSW_default=$(or $(and $(pyinc_DIR),$(WITH_SHARED_LIBRARIES),CYTHON),NO)

#py_DIR_default=/afs/slac/g/lcls/package/python/python2.7.9/$(TARCH)
#pyinc_DIR_default=$(addsuffix /include/python2.7/,$(py_DIR))
py_DIR_default=/afs/slac/g/lcls/package/python/3.6.1/$(TARCH)
pyinc_DIR_default=$(addsuffix /include/python3.6m/,$(py_DIR))

# Whether to use C++11 or boost (note that boost is still used internally
# by CPSW but enabling C++11 will remove dependency of *applications* on
# boost.
USE_CXX11_default=NO
# When enabling CXX11 then boost can completely be avoided by
# setting the following variable to NO. A potentially inefficient,
# trivial replacement for boost::lockfree:stack and boost::lockfree::queue
# will be used instead.
# This variable can be set for individual target architectures...
WITH_BOOST_default=YES

# Define an install location
INSTALL_DIR=$(TOPDIR)

# Optional command to post-process the script that is generated to
# set up LD_LIBRARY_PATH, PATH, PYTHONPATH.
# Such a script may be necessary in a cross-environment when paths
# as seen from the target differ from the installation paths as
# seen from the host...
POSTPROCESS_ENV_SCRIPT_default=true
