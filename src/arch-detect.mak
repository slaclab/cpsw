# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

#=================================================#
# Simple utility script to detect host arch
#=================================================#

HOST_ARCH:=$(shell uname -r | grep -Eo "el[0-9][0-9]?")
ifneq ($(HOST_ARCH),)
	HOST_ARCH:=rhel$(subst el,,$(HOST_ARCH))-$(shell uname -m)
else
	HOST_ARCH:=linux-$(shell uname -m)
endif

