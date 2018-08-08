CPSW_DIR=src
SRCDIR=.

-include $(SRCDIR)/release.mak
include $(CPSW_DIR)/defs.mak

SUBDIRS+= src
TGTS+=env

GENERATED_SRCS=env.slac.sh

include $(CPSW_DIR)/rules.mak

env: env-cpsw.sh

PYBINPATH=$(addsuffix /bin,$(abspath $(py_DIR)))
PYLIBPATH=$(addsuffix /lib,$(abspath $(py_DIR)))

env-cpsw.sh:
	$(RM) $@
	echo 'export $(LDINSTPATH)' > $@
	echo 'export LD_LIBRARY_PATH=$(PYLIBPATH)$${LD_LIBRARY_PATH:+:$${LD_LIBRARY_PATH}}' >> $@
	echo 'export PYTHONPATH=$(abspath $(INSTALL_DIR))/$(TARCH)/bin$${PYTHONPATH:+:$${PYTHONPATH}}' >> $@
	echo 'export PATH=$(abspath $(INSTALL_BIN))$(addprefix :,$(PYBINPATH))$${PATH:+:$${PATH}}' >> $@

clean_local:
	$(RM) env-cpsw.sh

install_local: env-cpsw.sh
	$(INSTALL) $^ $(INSTALL_DIR)/$(TARCH)/bin/

uninstall_local:
	$(RM) $(INSTALL_DIR)/$(TARCH)/bin/env-cpsw.sh

env.slac.sh: env.slac.sh.in .FORCE
	$(RM) $@
	sed -e 's%TOPDIR/%$(abspath $(INSTALL_DIR))/%' $< > $@

.PHONY: env .FORCE
