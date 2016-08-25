/* YAML Key definitions
 *
 * We don't want any macros in the 'visible' headers
 * but use this header just for sake of the test code
 * so that we don't have to rewrite test code with
 * hardcoded names to be rewritten if any name changes.
 */
#ifndef CPSW_YAML_KEYDEFS_H
#define CPSW_YAML_KEYDEFS_H

/* Use leading underscore for macros; C++ symbols don't have it */
#define _YAML_KEY_MERGE  "<<"
#define _YAML_KEY_at  "at"
#define _YAML_KEY_byteOrder  "byteOrder"
#define _YAML_KEY_cacheable  "cacheable"
#define _YAML_KEY_children  "children"
#define _YAML_KEY_class  "class"
#define _YAML_KEY_configBase  "configBase"
#define _YAML_KEY_configPrio  "configPrio"
#define _YAML_KEY_depack  "depack"
#define _YAML_KEY_description  "description"
#define _YAML_KEY_dynTimeout  "dynTimeout"
#define _YAML_KEY_encoding  "encoding"
#define _YAML_KEY_entry  "entry"
#define _YAML_KEY_enums  "enums"
#define _YAML_KEY_instantiate  "instantiate"
#define _YAML_KEY_ipAddr  "ipAddr"
#define _YAML_KEY_isSigned  "isSigned"
#define _YAML_KEY_ldFragWinSize  "ldFragWinSize"
#define _YAML_KEY_ldFrameWinSize  "ldFrameWinSize"
#define _YAML_KEY_lsBit  "lsBit"
#define _YAML_KEY_mode  "mode"
#define _YAML_KEY_name  "name"
#define _YAML_KEY_nelms  "nelms"
#define _YAML_KEY_numRxThreads  "numRxThreads"
#define _YAML_KEY_offset  "offset"
#define _YAML_KEY_outQueueDepth  "outQueueDepth"
#define _YAML_KEY_pollSecs  "pollSecs"
#define _YAML_KEY_port  "port"
#define _YAML_KEY_protocolVersion  "protocolVersion"
#define _YAML_KEY_retryCount  "retryCount"
#define _YAML_KEY_RSSI  "RSSI"
#define _YAML_KEY_sequence  "sequence"
#define _YAML_KEY_size  "size"
#define _YAML_KEY_sizeBits  "sizeBits"
#define _YAML_KEY_SRP  "SRP"
#define _YAML_KEY_SRPMux  "SRPMux"
#define _YAML_KEY_stride  "stride"
#define _YAML_KEY_stripHeader  "stripHeader"
#define _YAML_KEY_TDEST  "TDEST"
#define _YAML_KEY_TDESTMux  "TDESTMux"
#define _YAML_KEY_timeoutUS  "timeoutUS"
#define _YAML_KEY_UDP  "UDP"
#define _YAML_KEY_value  "value"
#define _YAML_KEY_virtualChannel  "virtualChannel"
#define _YAML_KEY_wordSwap  "wordSwap"

#endif
