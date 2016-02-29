# (Default) Definitions for CPSW Makefiles

# Architectures to build
ARCHES=host

# Save default (host) tools
CC_host    :=$(CC)
CXX_host   :=$(CXX)
AR_host    :=$(AR)
RANLIB_host:=$(or $(RANLIB),ranlib)

# if CROSS_xxx (for target architecture 'xxx' is not defined then try CROSS_default)
#
# For a host build this should be empty (since we assume the tools to be in PATH)
# but the user may override by defining CROSS_host...
CROSS=$(if $(findstring undefined,$(origin CROSS_$(TARCH))),$(CROSS_default),$(CROSS_$(TARCH)))

# Tools
CC     =$(CROSS)$(or $(CC_$(TARCH)),$(CC_default),gcc)
CXX    =$(CROSS)$(or $(CXX_$(TARCH)),$(CXX_default),g++)
AR     =$(CROSS)$(or $(AR_$(TARCH)),$(AR_default),ar)
RANLIB =$(CROSS)$(or $(RANLIB_$(TARCH)),$(RANLIB_default),ranlib)

# Tool options
OPT_CXXFLAGS=-g -Wall -O2
OPT_CFLAGS  =-g -Wall -O2

CXXFLAGS = $(addprefix -I,$(SRCDIR) $(INCLUDE_DIRS))
CXXFLAGS+= $(addprefix -I,$(subst :, ,$(cpswinc_DIRS)))
CXXFLAGS+= $(OPT_CXXFLAGS)
CXXFLAGS+= $(USR_CXXFLAGS) $(or $(USR_CXXFLAGS_$(TARCH)),$(USR_CXXFLAGS_default))

CFLAGS   = $(addprefix -I,$(SRCDIR) $(INCLUDE_DIRS))
CXXFLAGS+= $(addprefix -I,$(subst :, ,$(cpswinc_DIRS)))
CFLAGS  += $(OPT_CFLAGS)
CFLAGS  += $(USR_CFLAGS)   $(or $(USR_CFLAGS_$(TARCH)),$(USR_CFLAGS_default))

LDFLAGS  = $(USR_LDFLAGS)  $(or $(USR_LDFLAGS_$(TARCH)), $(USR_LDFLAGS_default))

VPATH=$(SRCDIR)

FILTERED_TBINS=$(filter-out $(DISABLED_TESTPROGRAMS),$(TESTPROGRAMS))

RUN_OPTS=''

# Libraries currently required by CPSW itself (and thus by anything using it)

# colon separated dirlist
cpswinc_DIRS=$(CPSW_DIR)$(addprefix :,$(or $(boostinc_DIR_$(TARCH)), $(boostinc_DIR_default), $(boostinc_DIR)))
# colon separated dirlist
cpswlib_DIRS=$(CPSW_DIR)/O.$(TARCH)$(addprefix :,$(or $(boostlib_DIR_$(TARCH)), $(boostlib_DIR_default), $(boostlib_DIR)))

# Libraries CPSW requires -- must be added to application's <prog>_LIBS variable
CPSW_LIBS   = cpsw boost_system pthread

# definitions
include $(CPSW_DIR)/config.mak
-include $(CPSW_DIR)/config.local.mak
-include $(SRCDIR)/local.mak
