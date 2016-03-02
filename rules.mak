# Rules for CPSW makefiles

# default target
multi-top: multi-install_headers multi-all
	@true

# run tests (on host)
test:sub-$(HARCH)-run_tests

multi-install_headers: install_headers

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

multi-%: multi-subdir-% multi-arch-%
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
# Argument 2 is the name of the library with '-' substituted by '_' (for use in variable names)
define LIBa_template

$(2)_OBJS=$$(patsubst %.cpp,%.o,$$(patsubst %.cc,%.o,$$(patsubst %.c,%.o,$$($(2)_SRCS))))

lib$(1).a: $$($(2)_OBJS)

SRCS+=$$($(2)_SRCS)

$(1):CXXFLAGS+=$$($(2)_CXXFLAGS) $$($(2)_CXXFLAGS_$$(TARNM))
$(1):CFLAGS  +=$$($(2)_CFLAGS)   $$($(2)_CFLAGS_$$(TARNM))

$(2)_CXXFLAGS_$$(TARNM)?= $$($(2)_CXXFLAGS_default)
$(2)_CFLAGS_$$(TARNM)  ?= $$($(2)_CFLAGS_default)

endef

TGTS+=$(STATIC_LIBRARIES:%=lib%.a)

# Expand the template for all library targets
$(foreach lib,$(STATIC_LIBRARIES),$(eval $(call LIBa_template,$(lib),$(subst -,_,$(lib)))))

$(STATIC_LIBRARIES:%=lib%.a):
	$(AR) cr $@ $^
	$(RANLIB) $@

# Template to link programs
# Argument 1 is the name of the program
# Argument 2 is the name of the program with '-' substituted by '_' (for use in variable names)
define PROG_template

$(2)_OBJS=$$(patsubst %.cpp,%.o,$$(patsubst %.cc,%.o,$$(patsubst %.c,%.o,$$($(2)_SRCS))))

$(1): $$($(2)_OBJS) $$(wildcard $$(call ADD_updir,$$(foreach lib,$$($(2)_LIBS),$$($$(subst -,_,$$(lib))_DIR_$$(TARNM))/lib$$(lib).a $$($$(subst -,_,$$(lib))_DIR)/lib$$(lib).a O.$$(TARCH)/lib$$(lib).a),))

SRCS+=$$($(2)_SRCS)

$(1):LDFLAGS +=$$($(2)_LDFLAGS)  $$($(2)_LDFLAGS_$$(TARNM))
$(1):CXXFLAGS+=$$($(2)_CXXFLAGS) $$($(2)_CXXFLAGS_$$(TARNM))
$(1):CFLAGS  +=$$($(2)_CFLAGS)   $$($(2)_CFLAGS_$$(TARNM))

$(2)_CXXFLAGS_$$(TARNM)?= $$($(2)_CXXFLAGS_default)
$(2)_CFLAGS_$$(TARNM)  ?= $$($(2)_CFLAGS_default)
$(2)_LDFLAGS_$$(TARNM) ?= $$($(2)_LDFLAGS_default)

endef

TGTS+=$(PROGRAMS)
TGTS+=$(TESTPROGRAMS)

# Expand the template for all program (and testprogram) targets
$(foreach bin,$(PROGRAMS) $(TESTPROGRAMS),$(eval $(call PROG_template,$(bin),$(subst -,_,$(bin)))))

$(PROGRAMS) $(TESTPROGRAMS):OBJS=$($(subst -,_,$@)_OBJS)
$(PROGRAMS) $(TESTPROGRAMS):LIBS=$($(subst -,_,$@)_LIBS)

$(PROGRAMS) $(TESTPROGRAMS): LIBARGS  = -L.
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(foreach lib,$(LIBS),$(addprefix -L,$(call ADD_updir,$($(subst -,_,$(lib))_DIR) $($(subst -,_,$(lib))_DIR_$(TARNM)),)))
# don't apply ADD_updir to cpswlib_DIRS because CPSW_DIR already was 'upped'.
# This means that e.g. boostlib_DIR must be absolute or relative to CPSW_DIR
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -L,$(subst :, ,$(cpswlib_DIRS)))
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -L,$(INSTALL_DIR:%=%/lib/$(TARCH)))
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(foreach lib,$(LIBS:%=-l%),$(lib:%.a=-Wl,-Bstatic % -Wl,-Bdynamic))

$(PROGRAMS) $(TESTPROGRAMS):
	$(CXX) -o $@ $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LIBARGS)

all: $(TGTS) install

run_tests: $(addsuffix _run,$(FILTERED_TBINS))
	@echo "ALL TESTS PASSED"

$(addsuffix _run,$(FILTERED_TBINS)):%_run: %
	@for opt in $(RUN_OPTS) ; do \
	    echo "Running ./$< $${opt}"; \
        if ( export LD_LIBRARY_PATH="$(cpswlib_DIRS)$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}"; ./$< $${opt} ) ; then \
			echo "TEST ./$< $${opt} PASSED" ; \
		else \
			echo "TEST ./$< $${opt} FAILED" ; \
			exit 1; \
		fi \
	done


ifneq ($(SRCS),)
deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $^ > $@
endif

clean: multi-subdir-clean
	$(RM) deps *.o *_tst udpsrv
	$(RM) -r O.*

install_headers: git_version_string.h
	@if [ -n "$(INSTALL_DIR)" ] ; then \
		echo "Installing Headers $(HEADERS)" ; \
		if [ -n "$(HEADERS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/include ;\
			$(INSTALL) $(addprefix $(SRCDIR:%=%/),$(HEADERS)) $(INSTALL_DIR)/include ;\
		fi ;\
	fi

install:
	@if [ -n "$(INSTALL_DIR)" ] ; then \
		if [ -n "$(STATIC_LIBRARIES)" ] ; then \
			mkdir -p $(INSTALL_DIR)/lib/$(TARCH) ;\
			echo "Installing Libraries $(STATIC_LIBRARIES:%=%lib.a)" ; \
			$(INSTALL) $(foreach lib,$(STATIC_LIBRARIES),$(lib:%=lib%.a)) $(INSTALL_DIR)/lib/$(TARCH) ;\
		fi ;\
		if [ -n "$(PROGRAMS)" ] ; then \
			mkdir -p $(INSTALL_DIR)/bin/$(TARCH) ;\
			echo "Installing Programs $(PROGRAMS)" ; \
			$(INSTALL) $(PROGRAMS) $(INSTALL_DIR)/bin/$(TARCH) ;\
		fi \
	fi

git_version_string.h: FORCE
	@if grep "$(GIT_VERSION_STRING)" $@ > /dev/null 2>&1 ; then \
		true ; \
	else \
		echo "VERSION REMADE" ; \
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
