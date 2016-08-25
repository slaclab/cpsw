# (Default) Definitions for CPSW Makefiles

# host architecture
HARCH=host
# Architectures to build
ARCHES=$(HARCH)

# Save default (host) tools
CC_host    :=$(CC)
CXX_host   :=$(CXX)
AR_host    :=$(AR)
RANLIB_host:=$(or $(RANLIB),ranlib)

GIT_VERSION_STRING:=$(shell echo -n '"'`git describe --always --dirty`'"')

# if CROSS_xxx (for target architecture 'xxx' is not defined then try CROSS_default)
#
# For a host build this should be empty (since we assume the tools to be in PATH)
# but the user may override by defining CROSS_host...
#
# arch2var(VARIABLE_PREFIX)
define arch2var
$(1)=$$(or $$($(1)_$$(TARNM)),$$($(1)_default))
endef

ARCHSPECIFIC_VARS+=CROSS
ARCHSPECIFIC_VARS+=yaml_cpp_DIR
ARCHSPECIFIC_VARS+=boost_DIR
ARCHSPECIFIC_VARS+=py_DIR
ARCHSPECIFIC_VARS+=WITH_SHARED_LIBRARIES
ARCHSPECIFIC_VARS+=WITH_STATIC_LIBRARIES
ARCHSPECIFIC_VARS+=WITH_PYCPSW
ARCHSPECIFIC_VARS+=BOOST_PYTHON_LIB
ARCHSPECIFIC_VARS+=XXX_TEST

$(foreach var,$(ARCHSPECIFIC_VARS),$(eval $(call arch2var,$(var))))

CROSS_default=$(addsuffix -,$(filter-out $(HARCH),$(TARCH)))

boostinc_DIR=$(if $(boost_DIR),$(boost_DIR)/include,)
boostlib_DIR=$(if $(boost_DIR),$(boost_DIR)/lib,)

yaml_cppinc_DIR=$(if $(yaml_cpp_DIR),$(yaml_cpp_DIR)/include,)
yaml_cpplib_DIR=$(if $(yaml_cpp_DIR),$(yaml_cpp_DIR)/lib,)

pyinc_DIR=$(if $(py_DIR),$(py_DIR)/include,)


# Tools
CC     =$(CROSS)$(or $(CC_$(TARNM)),$(CC_default),gcc)
CXX    =$(CROSS)$(or $(CXX_$(TARNM)),$(CXX_default),g++)
AR     =$(CROSS)$(or $(AR_$(TARNM)),$(AR_default),ar)
RANLIB =$(CROSS)$(or $(RANLIB_$(TARNM)),$(RANLIB_default),ranlib)
INSTALL=install -C

# Tool options
OPT_CXXFLAGS=-g -Wall -O2
OPT_CFLAGS  =-g -Wall -O2

CPPFLAGS+= $(addprefix -I,$(SRCDIR) $(INCLUDE_DIRS) $(INSTALL_DIR:%=%/include))
CPPFLAGS+= $(addprefix -I,$(subst :, ,$(cpswinc_DIRS)))
CPPFLAGS+= $(USR_CPPFLAGS) $(or $(USR_CPPFLAGS_$(TARNM)),$(USR_CPPFLAGS_default))
CXXFLAGS+= $(OPT_CXXFLAGS)
CXXFLAGS+= $(USR_CXXFLAGS) $(or $(USR_CXXFLAGS_$(TARNM)),$(USR_CXXFLAGS_default))
CFLAGS  += $(OPT_CFLAGS)
CFLAGS  += $(USR_CFLAGS)   $(or $(USR_CFLAGS_$(TARNM)),$(USR_CFLAGS_default))

LDFLAGS  = $(USR_LDFLAGS)  $(or $(USR_LDFLAGS_$(TARNM)), $(USR_LDFLAGS_default))

VPATH=$(SRCDIR)

FILTERED_TBINS=$(filter-out $(DISABLED_TESTPROGRAMS),$(TESTPROGRAMS))

RUN_OPTS=''

# Libraries currently required by CPSW itself (and thus by anything using it)

# colon separated dirlist
# Note: += adds a whitespace
cpswinc_DIRS=$(CPSW_DIR)$(addprefix :,$(or $(boostinc_DIR_$(TARNM)),$(boostinc_DIR_default),$(boostinc_DIR)))$(addprefix :,$(or $(yaml_cppinc_DIR_$(TARNM)),$(yaml_cppinc_DIR_default),$(yaml_cppinc_DIR)))
# colon separated dirlist
cpswlib_DIRS=$(addsuffix /O.$(TARCH),$(CPSW_DIR))$(addprefix :,$(or $(boostlib_DIR_$(TARNM)),$(boostlib_DIR_default),$(boostlib_DIR)))$(addprefix :,$(or $(yaml_cpplib_DIR_$(TARNM)),$(yaml_cpplib_DIR_default),$(yaml_cpplib_DIR)))

# Libraries CPSW requires -- must be added to application's <prog>_LIBS variable
CPSW_LIBS   = cpsw yaml-cpp pthread rt

# Default values -- DO NOT OVERRIDE HERE but in config.mak or config.local.mak
BOOST_PYTHON_LIB_default     =boost_python
WITH_SHARED_LIBRARIES_default=YES
WITH_STATIC_LIBRARIES_default=NO
WITH_PYCPSW_default          =$(WITH_SHARED_LIBRARIES_default)

COMMA__=,
SPACE__= #

# definitions
include $(CPSW_DIR)/config.mak
-include $(CPSW_DIR)/config.local.mak
-include $(SRCDIR)/local.mak
