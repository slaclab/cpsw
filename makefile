CPSW_DIR=src
SRCDIR=.

-include $(SRCDIR)/release.mak
include $(CPSW_DIR)/defs.mak

SUBDIRS = src

include $(CPSW_DIR)/rules.mak
