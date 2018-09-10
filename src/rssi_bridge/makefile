# //@C Copyright Notice
# //@C ================
# //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
# //@C file found in the top-level directory of this distribution and at
# //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
# //@C
# //@C No part of CPSW, including this file, may be copied, modified, propagated, or
# //@C distributed except according to the terms contained in the LICENSE.txt file.

CPSW_DIR=..

include $(CPSW_DIR)/defs.mak

INCLUDE_DIRS += $(SRCDIR)/../libTstAux

cpswTstAuxlib_DIR=$(SRCDIR)/../libTstAux/O.$(TARCH)/

DEP_HEADERS+=daemonize.h
DEP_HEADERS+=printErrMsg.h
DEP_HEADERS+=prot.h
DEP_HEADERS+=protRelayUtil.h


rssi_bridge_SRCS+= rssi_bridge.cc
rssi_bridge_SRCS+= daemonize.c
rssi_bridge_SRCS+= printErrMsg.c
rssi_bridge_SRCS+= protMap.c
rssi_bridge_SRCS+= protMap_procs.c
rssi_bridge_SRCS+= protRelay.c
rssi_bridge_SRCS+= protRelay_procs.c
rssi_bridge_SRCS+= prot_xdr.c
rssi_bridge_SRCS+= mcastSock.c
rssi_bridge_SRCS+= mapClnt.c
rssi_bridge_SRCS+= rpcMapServer.c
rssi_bridge_SRCS+= rpcMapLookup.c

rssi_bridge_LIBS_NO = cpswTstAux $(CPSW_LIBS)
rssi_bridge_LIBS_YES= cpswTstAux cpsw.a pthread rt
rssi_bridge_LIBS=$(rssi_bridge_LIBS_$(WITH_STATIC_LIBRARIES))

PROGRAMS = rssi_bridge
include $(CPSW_DIR)/rules.mak
