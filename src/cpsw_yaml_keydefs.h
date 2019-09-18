 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* YAML Key definitions
 */
#ifndef CPSW_YAML_KEYDEFS_H
#define CPSW_YAML_KEYDEFS_H

#define YAML_KEY_schemaVersionMajor    "schemaVersionMajor"
#define YAML_KEY_schemaVersionMinor    "schemaVersionMinor"
#define YAML_KEY_schemaVersionRevision "schemaVersionRevision"
#define YAML_KEY_MERGE  "<<"
#define YAML_KEY_align  "align"
#define YAML_KEY_at  "at"
#define YAML_KEY_byteOrder  "byteOrder"
#define YAML_KEY_cacheable  "cacheable"
#define YAML_KEY_children  "children"
#define YAML_KEY_class  "class"
#define YAML_KEY_configBase  "configBase"
#define YAML_KEY_configPrio  "configPrio"
#define YAML_KEY_cumulativeAckTimeoutUS "cumulativeAckTimeoutUS"
#define YAML_KEY_defaultWriteMode  "defaultWriteMode"
#define YAML_KEY_depack  "depack"
#define YAML_KEY_description  "description"
#define YAML_KEY_dynTimeout  "dynTimeout"
#define YAML_KEY_encoding  "encoding"
#define YAML_KEY_entry  "entry"
#define YAML_KEY_enums  "enums"
#define YAML_KEY_fileName "fileName"
#define YAML_KEY_instantiate  "instantiate"
#define YAML_KEY_ipAddr  "ipAddr"
#define YAML_KEY_isSigned  "isSigned"
#define YAML_KEY_ldFragWinSize  "ldFragWinSize"
#define YAML_KEY_ldFrameWinSize  "ldFrameWinSize"
#define YAML_KEY_ldMaxUnackedSegs "ldMaxUnackedSegs"
#define YAML_KEY_lsBit  "lsBit"
#define YAML_KEY_maxCumulativeAcks "maxCumulativeAcks"
#define YAML_KEY_maxRetransmissions "maxRetransmissions"
#define YAML_KEY_maxSegmentSize "maxSegmentSize"
#define YAML_KEY_mode  "mode"
#define YAML_KEY_name  "name"
#define YAML_KEY_nelms  "nelms"
#define YAML_KEY_nullTimeoutUS "nullTimeoutUS"
#define YAML_KEY_numRxThreads  "numRxThreads"
#define YAML_KEY_offset  "offset"
#define YAML_KEY_outQueueDepth  "outQueueDepth"
#define YAML_KEY_inpQueueDepth  "inpQueueDepth"
#define YAML_KEY_pollSecs  "pollSecs"
#define YAML_KEY_port  "port"
#define YAML_KEY_protocolVersion  "protocolVersion"
#define YAML_KEY_retryCount  "retryCount"
#define YAML_KEY_retransmissionTimeoutUS "retransmissionTimeoutUS"
#define YAML_KEY_RSSI  "RSSI"
#define YAML_KEY_rssiBridge  "rssiBridge"
#define YAML_KEY_seekable  "seekable"
#define YAML_KEY_sequence  "sequence"
#define YAML_KEY_singleInterfaceOnly  "singleInterfaceOnly"
#define YAML_KEY_size  "size"
#define YAML_KEY_sizeBits  "sizeBits"
#define YAML_KEY_socksProxy "socksProxy"
#define YAML_KEY_SRP  "SRP"
#define YAML_KEY_SRPMux  "SRPMux"
#define YAML_KEY_stride  "stride"
#define YAML_KEY_stripHeader  "stripHeader"
#define YAML_KEY_TDEST  "TDEST"
#define YAML_KEY_TDESTMux  "TDESTMux"
#define YAML_KEY_threadPriority "threadPriority"
#define YAML_KEY_timeoutUS  "timeoutUS"
#define YAML_KEY_UDP  "UDP"
#define YAML_KEY_TCP  "TCP"
#define YAML_KEY_value  "value"
#define YAML_KEY_virtualChannel  "virtualChannel"
#define YAML_KEY_wordSwap  "wordSwap"

#endif
