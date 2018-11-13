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
include $(CPSW_DIR)/std.mak

DEP_HEADERS += crc32-le-tbl-4.h
DEP_HEADERS += udpsrv_port.h
DEP_HEADERS += udpsrv_rssi_port.h
DEP_HEADERS += udpsrv_stream_state_monitor.h
DEP_HEADERS += udpsrv_util.h
DEP_HEADERS += udpsrv_cutil.h

#VPATH       += $(SRCDIR)/../
#VPATH       += $(SRCDIR)/../socks
#INCLUDES    += $(SRCDIR)/../
#INCLUDES    += $(SRCDIR)/../socks
cpswTstAux_SRCS += crc32-le-tbl-4.c
cpswTstAux_SRCS += udpsrv_port.cc
cpswTstAux_SRCS += udpsrv_util.cc
cpswTstAux_SRCS += udpsrv_stream_state_monitor.cc
#cpswTstAux_SRCS += cpsw_thread.cc
#cpswTstAux_SRCS += cpsw_event.cc
#cpswTstAux_SRCS += cpsw_obj_cnt.cc
#cpswTstAux_SRCS += cpsw_rssi.cc
#cpswTstAux_SRCS += cpsw_rssi_states.cc
#cpswTstAux_SRCS += cpsw_rssi_proto.cc
#cpswTstAux_SRCS += cpsw_rssi_timer.cc
#cpswTstAux_SRCS += cpsw_sock.cc

STATIC_LIBRARIES+=cpswTstAux

include $(CPSW_DIR)/rules.mak
