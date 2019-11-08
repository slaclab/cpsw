 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_FILE_LOOKUP_H
#define CPSW_FILE_LOOKUP_H

#include <unistd.h>
#include <string>
#include <list>
#include <stdio.h>

class FLookup {
private:
	typedef std::list<std::string> L;
	L    l_;
	bool tryAll_;
public:
	// construct initial list (':' separated paths).
	// NOTE: a NULL pathList is legal (empty list);
	//       useful when submitting getenv("XXX")...
	//
	// If 'tryAllNames' is true then even relative pathnames
	// containing a '/' are searched in the path list.
	FLookup(const char *pathList, bool tryAllNames = false);

	//
	FLookup &
	setTryAllNames(bool val)
	{
		tryAll_ = val;
		return *this;
	}

	// append a path to the list
	// RETURNS: reference to this object; useful for chaining
	//          appendPath("x").appendPath("y") ...
	FLookup &
	appendPath(const char *path);

	// prepend a path to the list
	// RETURNS: reference to this object; useful for chaining
	//          prependPath("x").prependPath("y") ...
	FLookup &
	prependPath(const char *path);

	// find a file in the list of paths and verify
	// that the calling process can access it in
	// 'mode' (see access(2)).
	// RETURNS: complete path
	// THROWS:  ErrnoError() if the file cannot be accessed.
	//          NotFoundError() if the file does not exist
	std::string
	lookup(const char *fileName, int mode = R_OK);

	std::string
	lookup(const std::string &fileName, int mode = R_OK)
	{
		return lookup( fileName.c_str() );
	}

	void
	dump(FILE *f);
};

#endif
