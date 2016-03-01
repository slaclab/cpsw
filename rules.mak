# Rules for CPSW makefiles

# default target
multi-all:

# run tests (on host)
test:sub-$(or $(findstring host,$(ARCHES)),$(firstword $(ARCHES)))-run_tests

# 'multi-target'; execute a target for all architectures:
# If the user has a target 'xxx' defined in his/her makefile
# then 'multi-xxx' builds that target in all arch-subdirs.
multi-%:$(patsubst %,sub-%-%,$(ARCHES))
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
$(foreach dir,$(1),$(addprefix $(if $(dir:/%=),$(UPDIR)),$(dir)))
endef

# recurse into subdirectory
sub-%:
	$(QUIET)mkdir -p O.$(TARCH)
	$(QUIET)$(MAKE) $(QUIET:%=-s) -C  O.$(TARCH) -f ../makefile SRCDIR=.. UPDIR=../ \
         CPSW_DIR="$(addprefix $(if $(CPSW_DIR:/%=),../),$(CPSW_DIR))" \
         INCLUDE_DIRS="$(foreach dir,$(INCLUDE_DIRS),$(addprefix $(if $(dir:/%=),../),$(dir)))" \
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

$(1): $$($(2)_OBJS) $$(wildcard $$(call ADD_updir,$$(foreach lib,$$($(2)_LIBS),$$($$(subst -,_,$$(lib))_DIR_$$(TARNM))/lib$$(lib).a $$($$(subst -,_,$$(lib))_DIR)/lib$$(lib).a O.$$(TARCH)/lib$$(lib).a)))

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
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(foreach lib,$(LIBS),$(addprefix -L,$(call ADD_updir,$($(subst -,_,$(lib))_DIR) $($(subst -,_,$(lib))_DIR_$(TARNM)))))
# don't apply ADD_updir to cpswlib_DIRS because CPSW_DIR already was 'upped'.
# This means that e.g. boostlib_DIR must be absolute or relative to CPSW_DIR
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -L,$(subst :, ,$(cpswlib_DIRS)))
$(PROGRAMS) $(TESTPROGRAMS): LIBARGS += $(addprefix -l,$(LIBS))

$(PROGRAMS) $(TESTPROGRAMS):
	$(CXX) -o $@ $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LIBARGS)

all: $(TGTS)

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


deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $^ > $@

clean:
	$(RM) deps *.o *_tst udpsrv
	$(RM) -r O.*

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
