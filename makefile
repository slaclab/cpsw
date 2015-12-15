
LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc
LSRCS+= cpsw_sval.cc
LSRCS+= cpsw_mmio_dev.cc
LSRCS+= cpsw_mem_dev.cc
LSRCS+= cpsw_nossi_dev.cc
LSRCS+= cpsw_buf.cc
LSRCS+= cpsw_proto_mod.cc
LSRCS+= cpsw_proto_mod_depack.cc

TSRCS = cpsw_path_tst.cc
TSRCS+= cpsw_sval_tst.cc
TSRCS+= cpsw_large_tst.cc
TSRCS+= cpsw_nossi_tst.cc
TSRCS+= cpsw_axiv_udp_tst.cc
TSRCS+= cpsw_buf_tst.cc

SRCS = $(LSRCS) $(TSRCS)

CXXFLAGS = -I. -g -Wall -O2
CFLAGS=-O2 -g

TBINS=$(patsubst %.cc,%,$(wildcard *_tst.cc))

TEST_AXIV_YES=
TEST_AXIV_NO=cpsw_axiv_udp_tst

FILTERED_TBINS=$(filter-out $(TEST_AXIV_$(TEST_AXIV)), $(TBINS))

all: tbins

tbins: $(TBINS)

test: $(FILTERED_TBINS)
	for i in $^ ; do \
		if ./$$i ; then \
			echo "TEST $$i PASSED" ; \
		else \
			echo "TEST $$i FAILED" ; \
			exit 1; \
		fi \
	done  ; \
	echo "TESTS PASSED"

cpsw_nossi_tst: udpsrv

cpsw_nossi_tst_LIBS+=-lboost_system -lpthread
cpsw_axiv_udp_tst_LIBS+=-lboost_system -lpthread

udpsrv: udpsrv.c
	$(CC) $(CFLAGS) -o $@ $^

%_tst: %_tst.cc libcpsw.a
	$(CXX) $(CXXFLAGS) -o $@ $< -L. -lcpsw $($@_LIBS)

libcpsw.a: $(LSRCS:%.cc=%.o)
	$(AR) cr $@ $^

deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $(SRCS) > $@

clean:
	$(RM) deps *.o *_tst udpsrv

-include deps
