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

# default target
all: multi-build
	@true

install: multi-build multi-do_install
	@true

# run tests (on host)
test: sub-$(HARCH)-run_tests

multi-clean: clean

# 'multi-target'; execute a target for all architectures:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-arch-xxx' builds that target in all arch-subdirs.
multi-arch-%: $(patsubst %,sub-%-%,$(ARCHES))
	@true

# 'multi-target'; execute a target for all subdirs:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-subdir-xxx' builds that target in all subdirs.
multi-subdir-%: $(patsubst %,sub-./%-%,$(SUBDIRS))
	@true

multi-%: generate_srcs install_headers multi-subdir-% multi-arch-%
	@true

# 'sub-x-y' builds target 'y' for architecture 'x'
# by recursively executing MAKE from subdir 'O.x 
# for target 'y'.

sub-%:TWORDS=$(subst -, ,$@)
sub-%:TARGT=$(lastword $(TWORDS))
sub-%:TARCH=$(patsubst %-$(TARGT),%,$(patsubst sub-%,%,$@))
sub-%:TARNM=$(subst -,_,$(TARCH))

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
sub-%-clean:
	@true

# No need to step into O.<arch> subdir to install headers
sub-%-install_headers:
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
$(foreach lib,$(STATIC_LIBRARIES),$(eval $(call LIBa_template,$(lib),$(subst .,_,$(subst -,_,$(lib))))))

$(foreach lib,$(SHARED_LIBRARIES),$(eval $(call LIBs_template,$(lib),$(subst .,_,$(subst -,_,$(lib))))))

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

# Template to link programs
# Argument 1 is the name of the program
# Argument 2 is the name of the program with '-' and '.' substituted by '_' (for use in variable names)
define PROG_template

$(2)_OBJS=$$(patsubst %.cpp,%.o,$$(patsubst %.cc,%.o,$$(patsubst %.c,%.o,$$($(2)_SRCS))))

$(1): $$($(2)_OBJS) $$(wildcard $$(foreach suff,.a .so .so.*,$$(addsuffix $$(suff), $$(call ADD_updir,$$(foreach lib,$$($(2)_LIBS),$$($$(subst .,_,$$(subst -,_,$$(lib)))_DIR_$$(TARNM))/lib$$(lib) $$($$(subst .,_,$$(subst -,_,$$(lib)))_DIR)/lib$$(lib)),) $$(foreach lib,$$($(2)_LIBS),lib$$(lib)))))

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

TGTS+=$(PROGRAMS)
TGTS+=$(TESTPROGRAMS)

# Expand the template for all program (and testprogram) targets
$(foreach bin,$(PROGRAMS) $(TESTPROGRAMS),$(eval $(call PROG_template,$(bin),$(subst .,_,$(subst -,_,$(bin))))))

$(PROGRAMS) $(TESTPROGRAMS):OBJS=$($(subst .,_,$(subst -,_,$@)_OBJS))
$(PROGRAMS) $(TESTPROGRAMS):LIBS=$($(subst .,_,$(subst -,_,$@)_LIBS))

$(PROGRAMS) $(TESTPROGRAMS): LIBARGS  = -L.
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(foreach lib,$(LIBS),$(addprefix -L,$(call ADD_updir,$($(subst .,_,$(subst -,_,$(lib)))_DIR) $($(subst .,_,$(subst -,_,$(lib)))_DIR_$(TARNM)),)))
# don't apply ADD_updir to cpswlib_DIRS because CPSW_DIR already was 'upped'.
# This means that e.g. yaml_cpplib_DIR must be absolute or relative to CPSW_DIR
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -L,$(subst :, ,$(cpswlib_DIRS)))
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -L,$(INSTALL_DIR:%=%/lib/$(TARCH)))
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(foreach lib,$(LIBS:%=-l%),$(lib:%.a=-Wl,-Bstatic % -Wl,-Bdynamic))

$(PROGRAMS) $(TESTPROGRAMS): $(STATIC_LIBRARIES:%=lib%.a) $(SHARED_LIBRARIES:%=lib%.so)
	$(CXX) -o $@ $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LIBARGS) $(addprefix -Wl$(COMMA__)-rpath$(COMMA__),$(abspath $(INSTALL_DIR:%=%/lib/$(TARCH))))

build: $(TGTS)

run_tests: $(addsuffix _run,$(FILTERED_TBINS))
	@echo "ALL TESTS PASSED"

$(addsuffix _run,$(FILTERED_TBINS)):%_run: %
	@for opt in $(RUN_OPTS) ; do \
	    echo "Running ./$< $${opt}"; \
        if ( LD_LIBRARY_PATH="$(cpswlib_DIRS)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" $(RUN_VARS) ./$< $${opt} ) ; then \
			echo "TEST ./$< $${opt} PASSED" ; \
		else \
			echo "TEST ./$< $${opt} FAILED" ; \
			exit 1; \
		fi; \
	done


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

clean: multi-subdir-clean
	$(RM) deps $(GENERATED_SRCS)
	$(RM) -r O.*

generate_srcs: $(GENERATED_SRCS)

install_headers: 
	@if [ -n "$(INSTALL_DIR)" ] ; then \
		echo "Installing Headers $(HEADERS)" ; \
		if [ -n "$(HEADERS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/include ;\
			$(INSTALL) $(addprefix $(SRCDIR:%=%/),$(HEADERS)) $(INSTALL_DIR)/include ;\
		fi ;\
	fi

do_install: install_headers
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

ifdef TARCH
-include deps
endif

# invoke with :  eval `make libpath`
ifndef TARCH
libpath:QUIET=@
libpath:sub-$(HARCH)-libpath
else
libpath:
	@echo export LD_LIBRARY_PATH="$(cpswlib_DIRS)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"
endif
