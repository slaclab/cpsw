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
