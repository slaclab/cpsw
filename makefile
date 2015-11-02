
LSRCS = cpsw_entry.cc cpsw_hub.cc cpsw_path.cc mmio_dev.cc cpsw_sval.cc
TSRCS = cpsw_path_tst.cc
SRCS = $(LSRCS) $(TSRCS)

CXXFLAGS = -I. -g

test: cpsw_path_tst

%_tst: %_tst.cc $(LSRCS:%.cc=%.o)
	$(CXX) $(CXXFLAGS) -o $@ $^


deps: $(SRCS)
	$(CXX) $(CXXFLAGS) -MM $(SRCS) > $@

clean:
	$(RM) deps *.o *_tst

-include deps
