
LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc mmio_dev.cc cpsw_sval.cc
TSRCS = cpsw_path_tst.cc cpsw_sval_tst.cc
SRCS = $(LSRCS) $(TSRCS)

CXXFLAGS = -I. -g -Wall

test: $(patsubst %.cc,%,$(wildcard *_tst.cc))

%_tst: %_tst.cc $(LSRCS:%.cc=%.o)
	$(CXX) $(CXXFLAGS) -o $@ $^


deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $(SRCS) > $@

clean:
	$(RM) deps *.o *_tst

-include deps
