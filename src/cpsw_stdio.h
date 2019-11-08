 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* Indirection of global I/O such as stdout/stderr/debug messages */
#ifndef CPSW_STDIO_H
#define CPSW_STDIO_H

#include <stdio.h>
#include <iostream>

namespace CPSW {
	// Error messages
	std::ostream &sErr();
	::FILE       *fErr();

	// Debug messages
	std::ostream &sDbg();
	::FILE       *fDbg();
};

#endif
