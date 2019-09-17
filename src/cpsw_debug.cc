 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
#include <cpsw_api_user.h>
#include <cpsw_debug.h>
#include <string.h>

struct Component {
	const char *name;
	int        *valp;
};

/* Table with known components */
static struct Component components[] = {
	{ "rssi"               , &cpsw_rssi_debug   },
	{ "proto_stack_builder", &cpsw_psbldr_debug },
	{ "socks",               &libSocksDebug     },
	{ "thread",              &cpsw_thread_debug },
	{                    0 ,                  0 }
};

int
setCPSWVerbosity(const char *component, int value)
{
struct Component *c, *f = 0;

	if ( component ) {
		for ( c = components; c->name; ++c ) {
			if ( strstr( c->name, component ) ) {
				if ( f ) {
					fprintf(stderr, "Requested name not specific enough\n");
					component = 0;
					f         = 0;
					break;
				}
				f = c;
			}
		}
	}

	if ( f ) {
		if ( ! f->valp ) {
			fprintf(stderr, "Debugging messages for '%s' not compiled in, sorry.\n", f->name);
			return -1;
		}
		*f->valp = value;
		return 0;
	} else {
		if ( component ) {
			fprintf(stderr, "'%s' not found.\n", component);
			component = 0;
		}
	}

	printf("setCPSWVerbosity -- known components are:\n");
	for ( c = components; c->name; ++c ) {
		printf("%s%s\n", c->name, c->valp ? "" : " (support not compiled in!)");
	}
	return 0;
}
