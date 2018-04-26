 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_MY_COMMAND_MOD_H
#define CPSW_MY_COMMAND_MOD_H

#include <cpsw_api_builder.h>

class IMyCommandImpl {
public:
	static Field create(const char *name, const char *connect_to);
};

#ifdef USE_FOR_TESTPROGRAM_ONLY

#ifdef __cplusplus
extern "C" {
#endif

typedef Field (*IMyCommandImpl_create)(const char *name, const char *connectTo);

#define IMYCOMMANDIMPL_CREATE "IMyCommandImpl_create"

#ifdef __cplusplus
};
#endif

#endif

#endif
