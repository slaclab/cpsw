# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

# Rules for CPSW makefiles

# The general pattern is that you invoke

# provide 'multi-' aliases for most common targets

# separator for makefile targets; targets have the form

# (sub | multi)-(<arch>|./<subdir>)@<target>
TSEP:=@

# default target
all: multi-build
	@true

install: multi-build multi-do_install
	@true

# run tests (on host only)
multi-test: sub-$(HARCH)$(TSEP)run_tests
	@true

multi-clean: clean
	@true

# 'multi-target'; execute a target for all architectures:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-arch-xxx' builds that target in all arch-subdirs.
multi-arch-%: $(patsubst %,sub-%$(TSEP)%,$(ARCHES))
	@true

# 'multi-target'; execute a target for all subdirs:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-subdir-xxx' builds that target in all subdirs.
multi-subdir-%: $(patsubst %,sub-./%$(TSEP)%,$(SUBDIRS))
	@true

# 'multi-target'; execute a target for all subdirs:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-postsubdir-xxx' builds that target in all subdirs.
multi-postsubdir-%: $(patsubst %,sub-./%$(TSEP)%,$(POSTBUILD_SUBDIRS))
	@true

multi-%: generate_srcs install_headers multi-subdir-% multi-arch-% multi-postsubdir-%
	@true

# convert '.' and '-' in a name to '_' since the former must not
# be present in a gnu-make variable name.
define nam2varnam
$(subst .,_,$(subst -,_,$(1)))
endef

# 'sub-x$(TSEP)y' builds target 'y' for architecture 'x'
# by recursively executing MAKE from subdir 'O.x
# for target 'y'.

sub-%:TWORDS=$(subst $(TSEP), ,$@)
sub-%:TARGT=$(lastword $(TWORDS))
sub-%:TARCH=$(patsubst %$(TSEP)$(TARGT),%,$(patsubst sub-%,%,$@))
sub-%:TARNM=$(call nam2varnam,$(TARCH))

# Function which checks a list of words (supposedly directories)
# and prepends $(UPDIR) to each one of them that doesn't start
# with '/' (-> relative paths may receive a ../ prefix)
#
# We use this when running from a target subdirectory.
#
# E.g., $(call ADD_updir,/a/b/c ../e) expands to  '/a/b/c $(UPDIR)../e'
# which (with UPDIR set to ../ expands to /a/b/c ../../e)
#
define ADD_updir
$(foreach dir,$(1),$(addprefix $(if $(dir:/%=),$(or $(2),$(UPDIR))),$(dir)))
endef

# recurse into subdirectory

# subdir make
sub-./%:
	$(QUIET)$(MAKE) $(QUIET:%=-s) -C  $(TARCH)  UPDIR=../$(UPDIR) \
         CPSW_DIR="$(call ADD_updir,$(CPSW_DIR),../)" \
         INSTALL_DIR="$(call ADD_updir,$(INSTALL_DIR),../)" \
         "multi-$(TARGT)"

# No need to step into O.<arch> subdir to clean; it will be removed anyways
sub-%$(TSEP)clean:
	@true

# No need to step into O.<arch> subdir to install headers
sub-%$(TSEP)install_headers:
	@true

# arch-subdir make
sub-%:
	$(QUIET)mkdir -p O.$(TARCH)
	$(QUIET)$(MAKE) $(QUIET:%=-s) -C  O.$(TARCH) -f ../makefile SRCDIR=.. UPDIR=../$(UPDIR) \
         CPSW_DIR="$(call ADD_updir,$(CPSW_DIR),../)" \
         INCLUDE_DIRS="$(call ADD_updir,$(INCLUDE_DIRS),../)" \
         INSTALL_DIR="$(call ADD_updir,$(INSTALL_DIR),../)" \
         TARNM="$(TARNM)" \
         TARCH="$(TARCH)" \
         "$(TARGT)"


# Template to assemble static libraries
# Argument 1 is the name of the library (*without* 'lib' prefix or '.a' suffix)
# Argument 2 is the name of the library with '-' and '.' substituted by '_' (for use in variable names)
define LIBa_template

$(2)_OBJS=$$(patsubst %.cpp,%.o,$$(patsubst %.cc,%.o,$$(patsubst %.c,%.o,$$($(2)_SRCS))))

lib$(1).a: $$($(2)_OBJS)

SRCS+=$$($(2)_SRCS)

# when computing dependencies (.dp) for source files we want to add target_specific CPPFLAGS (-I)
$(1) $(addsuffix .dp, $(basename $($(2)_SRCS))):CPPFLAGS+=$$($(2)_CPPFLAGS) $$($(2)_CPPFLAGS_$$(TARNM))
$(1):CXXFLAGS+=$$($(2)_CXXFLAGS) $$($(2)_CXXFLAGS_$$(TARNM))
$(1):CFLAGS  +=$$($(2)_CFLAGS)   $$($(2)_CFLAGS_$$(TARNM))

$(2)_CPPFLAGS_$$(TARNM)?= $$($(2)_CPPFLAGS_default)
$(2)_CXXFLAGS_$$(TARNM)?= $$($(2)_CXXFLAGS_default)
$(2)_CFLAGS_$$(TARNM)  ?= $$($(2)_CFLAGS_default)

endef

# Template to assemble shared libraries
# Argument 1 is the name of the library (*without* 'lib' prefix or '.so' suffix)
# Argument 2 is the name of the library with '-' and '.' substituted by '_' (for use in variable names)
define LIBs_template

$(2)_OBJS=$$(patsubst %.cpp,%.pic.o,$$(patsubst %.cc,%.pic.o,$$(patsubst %.c,%.pic.o,$$($(2)_SRCS))))

lib$(1).so: $$($(2)_OBJS)

SRCS+=$$($(2)_SRCS)

# when computing dependencies (.dp) for source files we want to add target_specific CPPFLAGS (-I)
$(1) $(addsuffix .dp, $(basename $($(2)_SRCS))):CPPFLAGS+=$$($(2)_CPPFLAGS) $$($(2)_CPPFLAGS_$$(TARNM))
$(1):CXXFLAGS+=$$($(2)_CXXFLAGS) $$($(2)_CXXFLAGS_$$(TARNM))
$(1):CFLAGS  +=$$($(2)_CFLAGS)   $$($(2)_CFLAGS_$$(TARNM))

$(2)_CPPFLAGS_$$(TARNM)?= $$($(2)_CPPFLAGS_default)
$(2)_CXXFLAGS_$$(TARNM)?= $$($(2)_CXXFLAGS_default)
$(2)_CFLAGS_$$(TARNM)  ?= $$($(2)_CFLAGS_default)

endef

TGTS+=$(STATIC_LIBRARIES:%=lib%.a) $(SHARED_LIBRARIES:%=lib%.so)

# Expand the template for all library targets
$(foreach lib,$(STATIC_LIBRARIES),$(eval $(call LIBa_template,$(lib),$(call nam2varnam,$(lib)))))

$(foreach lib,$(SHARED_LIBRARIES),$(eval $(call LIBs_template,$(lib),$(call nam2varnam,$(lib)))))

$(STATIC_LIBRARIES:%=lib%.a):
	$(AR) cr $@ $^
	$(RANLIB) $@

%.pic.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fpic -o $@ -c $<

%.pic.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fpic -o $@ -c $<

%.pic.o: %.c
	$(CC) $(CPPFLAGS) $(CLAGS) -fpic -o $@ -c $<

$(SHARED_LIBRARIES:%=lib%.so):
	$(CXX) -shared -o $@ $^

# Find library with <stem> passed as the first argument.
# Return path to 'lib<stem>.so' or 'lib<stem>.a':
# if 'stem' is of the form '%.a' then only a static version
# is searched. Otherwise a static version is only searched
# if a shared version cannot be found
define findlib
$(or $(if $(filter-out %.a,$(1)),$(call searchlib,$(1),lib$(1).so)),$(call searchlib,$(1),$(or $(addprefix lib,$(filter %.a,$(1))),lib$(1).a)))
endef

# Try to actually locate the library
define searchlib
$(wildcard $(addsuffix /$(2),$($(call nam2varnam,$(1))lib_DIR) $(subst :, ,$(cpswlib_DIRS)) .))
endef


# Template to link programs
# Argument 1 is the name of the program
# Argument 2 is the name of the program with '-' and '.' substituted by '_' (for use in variable names)
define PROG_template

$(2)_OBJS=$$(patsubst %.cpp,%.o,$$(patsubst %.cc,%.o,$$(patsubst %.c,%.o,$$($(2)_SRCS))))
$(2)_LIBS_WITH_PATH=$$(foreach lib,$$($(2)_LIBS),$$(call findlib,$$(lib)))

$(1): $$($(2)_OBJS) $$($(2)_LIBS_WITH_PATH)

SRCS+=$$($(2)_SRCS)

$(1):LDFLAGS +=$$($(2)_LDFLAGS)  $$($(2)_LDFLAGS_$$(TARNM))

# when computing dependencies (.dp) for source files we want to add target_specific CPPFLAGS (-I)
$(1) $(addsuffix .dp, $(basename $($(2)_SRCS))):CPPFLAGS+=$$($(2)_CPPFLAGS) $$($(2)_CPPFLAGS_$$(TARNM))
$(1):CXXFLAGS+=$$($(2)_CXXFLAGS) $$($(2)_CXXFLAGS_$$(TARNM))
$(1):CFLAGS  +=$$($(2)_CFLAGS)   $$($(2)_CFLAGS_$$(TARNM))

# special flags for shared objects
ifneq ($$(filter %.so,$(1)),)
$(1):LDFLAGS  += -shared
$(1):CXXFLAGS += -fpic
$(1):CFLAGS   += -fpic
endif

$(2)_CPPFLAGS_$$(TARNM)?= $$($(2)_CPPFLAGS_default)
$(2)_CXXFLAGS_$$(TARNM)?= $$($(2)_CXXFLAGS_default)
$(2)_CFLAGS_$$(TARNM)  ?= $$($(2)_CFLAGS_default)
$(2)_LDFLAGS_$$(TARNM) ?= $$($(2)_LDFLAGS_default)

endef

BINTGTS=$(PROGRAMS) $(TESTPROGRAMS) $(SHARED_OBJS)

TGTS+=$(BINTGTS)

# Expand the template for all program (and testprogram) targets
$(foreach bin,$(BINTGTS),$(eval $(call PROG_template,$(bin),$(call nam2varnam,$(bin)))))

$(BINTGTS):OBJS=$($(call nam2varnam,$@)_OBJS)
$(BINTGTS):LIBS=$($(call nam2varnam,$@)_LIBS)

$(BINTGTS): LIBARGS  = -L.
$(BINTGTS): LIBARGS += $(foreach lib,$(LIBS),$(addprefix -L,$($(call nam2varnam,$(lib))lib_DIR)))
# don't apply ADD_updir to cpswlib_DIRS because CPSW_DIR already was 'upped'.
# This means that e.g. yaml_cpplib_DIR must be absolute or relative to CPSW_DIR
$(BINTGTS): LIBARGS += $(addprefix -L,$(subst :, ,$(cpswlib_DIRS)))
$(BINTGTS): LIBARGS += $(addprefix -L,$(INSTALL_DIR:%=%/lib/$(TARCH)))
$(BINTGTS): LIBARGS += $(foreach lib,$(LIBS:%=-l%),$(lib:%.a=-Wl,-Bstatic % -Wl,-Bdynamic))

$(BINTGTS): $(STATIC_LIBRARIES:%=lib%.a) $(SHARED_LIBRARIES:%=lib%.so)
	$(CXX) -o $@ $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LIBARGS) $(addprefix -Wl$(COMMA__)-rpath$(COMMA__),$(abspath $(INSTALL_DIR:%=%/lib/$(TARCH))))

build: $(TGTS)

run_tests: $(addsuffix _run,$(FILTERED_TBINS))
	@echo "ALL TESTS PASSED"

define make_ldlib_path
$(subst $(SPACE__),:,$(strip . $(dir $(filter %.so,$(1)))))
endef


$(addsuffix _run,$(FILTERED_TBINS)):%_run: %
	@for opt in $(RUN_OPTS) ; do \
	    echo "Running ./$< $${opt}"; \
        if ( LD_LIBRARY_PATH="$(call make_ldlib_path,$($^_LIBS_WITH_PATH))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" $(RUN_VARS) ./$< $${opt} ) ; then \
			echo "TEST ./$< $${opt} PASSED" ; \
		else \
			echo "TEST ./$< $${opt} FAILED" ; \
			exit 1; \
		fi; \
	done

$(addsuffix _pat,$(FILTERED_TBINS)):%_pat: %
	echo "'$(subst $(SPACE__),:,$(dir $(filter %.so,$($^_LIBS_WITH_PATH))))'"


# We only track changes in .c and .cc files (and the .h files
# they depend on.
ifneq ($(SRCS),)
deps: $(addsuffix .dp, $(basename $(SRCS)))
	$(RM) $@
	cat $^ > $@
endif

%.dp:%.cc $(DEP_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MM -MT "$(addprefix $(@:%.dp=%),.o .pic.o)" $< > $@

%.dp:%.c $(DEP_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM -MT "$(addprefix $(@:%.dp=%),.o .pic.o)" $< > $@

clean: multi-subdir-clean multi-postsubdir-clean clean_local
	$(RM) deps $(GENERATED_SRCS)
	$(RM) -r O.*

generate_srcs: $(GENERATED_SRCS)
	@true

install_headers:
	@if [ -n "$(INSTALL_DIR)" ] ; then \
		echo "Installing Headers $(HEADERS)" ; \
		if [ -n "$(HEADERS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/include ;\
			$(INSTALL) $(addprefix $(SRCDIR:%=%/),$(HEADERS)) $(INSTALL_DIR)/include ;\
		fi ;\
	fi

do_install: install_headers install_local
	@if [ -n "$(INSTALL_DIR)" ] ; then \
		if [ -n "$(STATIC_LIBRARIES)" ] ; then \
			mkdir -p $(INSTALL_DIR)/lib/$(TARCH) ;\
			echo "Installing Libraries $(STATIC_LIBRARIES:%=%lib.a)" ; \
			$(INSTALL) $(foreach lib,$(STATIC_LIBRARIES),$(lib:%=lib%.a)) $(INSTALL_DIR)/lib/$(TARCH) ;\
		fi ;\
		if [ -n "$(SHARED_LIBRARIES)" ] ; then \
			mkdir -p $(INSTALL_DIR)/lib/$(TARCH) ;\
			echo "Installing Libraries $(SHARED_LIBRARIES:%=%lib.so)" ; \
			$(INSTALL) $(foreach lib,$(SHARED_LIBRARIES),$(lib:%=lib%.so)) $(INSTALL_DIR)/lib/$(TARCH) ;\
		fi ;\
		if [ -n "$(PROGRAMS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/bin/$(TARCH) ;\
			echo "Installing Programs $(PROGRAMS)" ; \
			$(INSTALL) $(PROGRAMS) $(INSTALL_DIR)/bin/$(TARCH) ;\
		fi ;\
		if [ -n "$(SHARED_OBJS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/lib/$(TARCH) ;\
			echo "Installing Shared Objects $(SHARED_OBJS)" ; \
			$(INSTALL) $(SHARED_OBJS) $(INSTALL_DIR)/lib/$(TARCH) ;\
		fi \
	fi

git_version_string.h: FORCE
	@if grep '$(GIT_VERSION_STRING)' $@ > /dev/null 2>&1 ; then \
		true ; \
	else \
		$(RM) $@ ; \
		echo '#ifndef GIT_VERSION_STRING_H' >  $@ ;\
		echo '#define GIT_VERSION_STRING_H' >> $@ ;\
		echo '#define GIT_VERSION_STRING "'$(GIT_VERSION_STRING)'"' >> $@ ;\
		echo '#endif' >> $@ ; \
	fi

FORCE:

.PHONY: install_local install_headers clean_local

ifdef TARCH
-include deps
endif

# invoke with :  eval `make libpath`
ifndef TARCH
libpath:QUIET=@
libpath:sub-$(HARCH)$(TSEP)libpath
else
libpath:
	@echo export LD_LIBRARY_PATH="$(subst $(SPACE__),:,$(patsubst ../%,%,$(subst :, ,$(cpswlib_DIRS))))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"
endif
