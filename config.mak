# In this file the desired target architectures
# as well as toolchain paths and prefixes are
# defined.
#
# The names for target architectures can be picked
# arbitrarily but should not characters besides
# [0-9][a-z][A-Z][.][-][_]

# By default the 'host' architecture is built.

# Assuming you also want 'xxx' and 'yyy' then you'd
# add these to the 'ARCHES' variable:

# ARCHES += xxx yyy

# Next, you need to define prefixes (which may include
# absolute paths) for the tools, e.g.,:

# CROSS_xxx = path_to_xxx_tools/xxx-
# CROSS_yyy = path_to_yyy_tools/yyy-

# so that e.g., $(CROSS_xxx)gcc can be used as a C compiler

# (you can override 'gcc' by another compiler by setting CC_<arch>)


# CPSW requires 'boost'. If this installed at a non-standard
# location (where it cannot be found automatically by the tools)
# then you need to set one of the variables

# boostinc_DIR_xxx, boostinc_DIR_default, boostinc_DIR

# to the directory where boost headers are installed.
# These aforementioned variables are tried in the listed
# order and the first one which is defined is used.
# Thus, e.g.,
#
# boostinc_DIR_xxx = /a/b/c
# boostinc_DIR     = /e/f
#
# will use /a/b/c for architecture 'xxx' and /e/f for
# any other one (including the 'host').

# Likewise, you must set
# boostlib_DIR_xxx, boostlib_DIR_default, boostlib_DIR
# if 'boost' libraries cannot be found automatically by
# the (cross) tools.
# Note 'boostlib_DIR' (& friends) must be absolute paths
# or relative to $(CPSW_DIR).
