SRCDIR=.

ARCHES=host
TARCH=host

CC_host:=$(CC)
CXX_host:=$(CXX)
AR_host:=$(AR)

BOOSTINCP=$(BOOSTINCP_$(TARCH))
BOOSTLIBP=$(BOOSTLIBP_$(TARCH))

CC=$(CC_$(TARCH))
CXX=$(CXX_$(TARCH))
AR=$(AR_$(TARCH))

#include local definitions

# mak.local should define desired
# target architectures and respective
# toolchains. E.g.,

# ARCHES=xxx yyy
# CC_xxx=<path-to-CC-for-target-xxx>
# CC_yyy=<path-to-CC-for-target-yyy>
# BOOSTINCP_xxx=-I<path-to-boost-includes-for-target-xxx>
# BOOSTINCP_yyy=-I<path-to-boost-includes-for-target-yyy>

-include $(SRCDIR)/mak.local

LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
LSRCS+= cpsw_entry_adapt.cc
LSRCS+= cpsw_sval.cc
LSRCS+= cpsw_mmio_dev.cc
LSRCS+= cpsw_mem_dev.cc
LSRCS+= cpsw_nossi_dev.cc
LSRCS+= cpsw_buf.cc
LSRCS+= cpsw_proto_mod.cc
LSRCS+= cpsw_proto_mod_depack.cc
LSRCS+= cpsw_proto_mod_udp.cc
LSRCS+= cpsw_proto_mod_srpmux.cc

TSRCS = cpsw_path_tst.cc
TSRCS+= cpsw_sval_tst.cc
TSRCS+= cpsw_large_tst.cc
TSRCS+= cpsw_nossi_tst.cc
TSRCS+= cpsw_axiv_udp_tst.cc
TSRCS+= cpsw_buf_tst.cc
TSRCS+= cpsw_stream_tst.cc
TSRCS+= cpsw_srpmux_tst.cc

ASRCS+= crc32-le-tbl-4.c

MSRCS+=udpsrv_mod_mem.cc
MSRCS+=udpsrv_mod_axiprom.cc

SRCS = $(LSRCS) $(TSRCS) $(ASRCS) $(MSRCS)

CXXFLAGS = -I$(SRCDIR) $(BOOSTINCP) -g -Wall -O2
CFLAGS=-O2 -g -I$(SRCDIR)

VPATH=$(SRCDIR)

TBINS=$(patsubst $(SRCDIR)/%.cc,%,$(wildcard $(SRCDIR)/*_tst.cc))

TEST_AXIV_YES=
TEST_AXIV_NO=cpsw_axiv_udp_tst

FILTERED_TBINS=$(filter-out $(TEST_AXIV_$(TEST_AXIV)), $(TBINS))

# may override for individual test targets which will be
# executed with for all option sets, eg:
#
#    some_tst_run:RUN_OPTS='' '-a -b'
#
# (will run once w/o opts, a second time with -a -b)
RUN_OPTS=''

# run for V2 and V1
cpsw_nossi_tst_run:     RUN_OPTS='' '-V1 -p8191'

cpsw_srpmux_tst_run:    RUN_OPTS='' '-V1 -p8191'

cpsw_axiv_udp_tst_run:  RUN_OPTS='' '-S 30'

# error percentage should be ~double of the value used for udpsrv (-L)
cpsw_stream_tst_run:    RUN_OPTS='-e 10'

# default target
multi-all:


multi-%:$(patsubst %,sub-%-%,$(ARCHES))
	true


sub-%:TWORDS=$(subst -, ,$@)
sub-%:TARCH=$(patsubst %-$(lastword $(TWORDS)),%,$(patsubst sub-%,%,$@))
sub-%:TARGT=$(lastword 3,$(subst -, ,$@))

sub-%:
	mkdir -p O.$(TARCH)
	make -C O.$(TARCH) -f ../makefile SRCDIR=.. TARCH=$(subst -,_,$(TARCH)) $(TARGT)


all: tbins

tbins: $(TBINS)

test: $(addsuffix _run,$(FILTERED_TBINS))
	@echo "ALL TESTS PASSED"

%_tst_run: %_tst
	@for opt in $(RUN_OPTS) ; do \
	    echo "Running ./$< $${opt}"; \
		if ./$< $${opt} ; then \
			echo "TEST ./$< $${opt} PASSED" ; \
		else \
			echo "TEST ./$< $${opt} FAILED" ; \
			exit 1; \
		fi \
	done

cpsw_nossi_tst: udpsrv

cpsw_nossi_tst_LIBS+=-lboost_system -lpthread
cpsw_axiv_udp_tst_LIBS+=-lboost_system -lpthread
cpsw_stream_tst_LIBS+=-lboost_system -lpthread -ltstaux
cpsw_srpmux_tst_LIBS+=-lboost_system -lpthread

udpsrv: udpsrv.o $(MSRCS:%.cc=%.o) libtstaux.a
	$(CXX) $(CFLAGS) -o $@ $^ -L. -lpthread -ltstaux

%_tst: %_tst.cc libcpsw.a libtstaux.a
	$(CXX) $(CXXFLAGS) -o $@ $< -L. $(BOOSTLIBP) -lcpsw $($@_LIBS)

libcpsw.a: $(LSRCS:%.cc=%.o)
	$(AR) cr $@ $^

libtstaux.a: $(ASRCS:%.cc=%.o) $(ASRCS:%.c=%.o)
	$(AR) cr $@ $^

deps: $(SRCS) udpsrv.c
	$(CXX) $(CXXFLAGS) -MM $^ > $@

clean:
	$(RM) deps *.o *_tst udpsrv
	$(RM) -r O.*

-include deps

# invoke with :  eval `make libpath`
libpath:
	@echo export LD_LIBRARY_PATH=$(BOOSTLIBP:-L%=%)
