 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <cpsw_flookup.h>
#include <string.h>
#include <stdlib.h>
#include <cpsw_error.h>

FLookup::FLookup(const char *envVarName)
{
const char *so;
char  *s = 0;
char *col;
char *b;

	if ( (so = getenv( envVarName )) ) {
		s = strdup( so );
		for ( b = s; col = strchr(b, ':'); b = col+1 ) {
			if ( col == b ) {
				/* initial ':' or '::' */
				appendPath( "." );
			} else {
				*col = 0;
				appendPath( b   );
			}
		}

		if ( *b ) {
			// last element (no trailing ':') 
			appendPath( b );
		} else {
			/* A trailing colon leads to 'b' pointing at the
			 * terminating "NUL" -- unless 's' had been empty
			 * in the first place!
			 */
			if ( b != s ) {
				// trailing colon
				appendPath( "." );
			}
		}
	}

	free( s );
}

void
FLookup::appendPath(const char *path)
{
	if ( ! *path )
		return;

	if ( path[strlen(path) - 1] != '/' ) {
		l_.push_back( std::string( path ) + "/" );
	} else {
		l_.push_back( path );
	}
}

void
FLookup::prependPath(const char *path)
{
	if ( ! *path )
		return;

	if ( path[strlen(path) - 1] != '/' ) {
		l_.push_front( std::string( path ) + "/" );
	} else {
		l_.push_front( path );
	}
}

void
FLookup::dump(FILE *f)
{
L::const_iterator it;

	for ( it = l_.begin(); it != l_.end(); ++it ) {
		fprintf(f, "%s\n", it->c_str());
	}
}

std::string
FLookup::lookup(const char *fileName, int mode)
{
	if ( strchr( fileName, '/' ) ) {
		/* don't consult the list */
		if ( access( fileName, mode ) ) {
			if ( ENOENT != errno ) {
				throw ErrnoError( std::string("FLookup: unable to access '") + fileName + "'" );
			}
		}
		return fileName;
	} else {
		L::const_iterator it;
		for ( it = l_.begin(); it != l_.end(); ++it ) {
			std::string rval = *it + fileName;
			if ( 0 == access( rval.c_str(), mode ) ) {
				return rval;
			} else if ( ENOENT != errno ) {
				throw ErrnoError( std::string("FLookup: unable to access '") + fileName + "'" );
			}
		}	
	}
	throw NotFoundError( std::string("FLookup: '") + fileName + "' not found" );
}
