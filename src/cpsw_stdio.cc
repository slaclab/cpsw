 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

/* Indirection of global I/O such as stdout/stderr/debug messages */
#include <cpsw_stdio.h>

namespace CPSW {
	// Error messages
	std::ostream &sErr()
	{
		return std::cerr;
	}

	::FILE       *fErr()
	{
		return ::stderr;
	}

	// Debug messages
	std::ostream &sDbg()
	{
		return std::cerr;
	}

	::FILE       *fDbg()
	{
		return ::stderr;
	}
};
