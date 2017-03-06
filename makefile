TOPTARGETS := all clean install uninstall
SUBDIRS := src 

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	cd $@ && $(MAKE) $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS) 
