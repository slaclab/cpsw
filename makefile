
#HARCH=linux-x86_64
#BOOSTINCP=-I/afs/slac/g/lcls/package/boost/1.57.0/$(HARCH)/include
#BOOSTLIBP=-L/afs/slac/g/lcls/package/boost/1.57.0/$(HARCH)/lib

#include local definitions
-include mak.local

LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
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

SRCS = $(LSRCS) $(TSRCS)

CXXFLAGS = -I. $(BOOSTINCP) -g -Wall -O2
CFLAGS=-O2 -g -I.

TBINS=$(patsubst %.cc,%,$(wildcard *_tst.cc))

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

all: tbins

tbins: $(TBINS)

test: $(addsuffix _run,$(FILTERED_TBINS))
	@echo "ALL TESTS PASSED"

%_tst_run: %_tst
	@for opt in $(RUN_OPTS) ; do \
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
cpsw_stream_tst_LIBS+=-lboost_system -lpthread
cpsw_srpmux_tst_LIBS+=-lboost_system -lpthread

udpsrv: udpsrv.c
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

%_tst: %_tst.cc libcpsw.a
	$(CXX) $(CXXFLAGS) -o $@ $< -L. $(BOOSTLIBP) -lcpsw $($@_LIBS)

libcpsw.a: $(LSRCS:%.cc=%.o)
	$(AR) cr $@ $^

deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $(SRCS) > $@

clean:
	$(RM) deps *.o *_tst udpsrv

-include deps

# invoke with :  eval `make libpath`
libpath:
	@echo export LD_LIBRARY_PATH=$(BOOSTLIBP:-L%=%)
