
LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc cpsw_mmio_dev.cc cpsw_sval.cc cpsw_nossi_dev.cc
LSRCS+= cpsw_mem_dev.cc
TSRCS = cpsw_path_tst.cc cpsw_sval_tst.cc cpsw_large_tst.cc cpsw_nossi_tst.cc cpsw_axiv_udp_tst.cc
SRCS = $(LSRCS) $(TSRCS)

CXXFLAGS = -I. -g -Wall -O2
CFLAGS=-O2 -g

test: $(patsubst %.cc,%,$(wildcard *_tst.cc))

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
