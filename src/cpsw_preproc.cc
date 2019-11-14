 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.


#include <cpsw_api_builder.h>
#include <cpsw_preproc.h>
#include <cpsw_yaml_keydefs.h>
#include <cpsw_stdio.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>

#define YAML_PATH "YAML_PATH"

StreamMuxBuf::StreamEl::StreamEl(Stream stream, const std::string *name, unsigned hdrLns)
: stream       ( stream                       ),
  name         ( name ? *name : std::string() ),
  headerLines  ( hdrLns                       )
{
}


StreamMuxBuf::StreamMuxBuf(unsigned bufsz)
: bufsz_          ( bufsz ),
  done_           ( false ),
  nlSoFar_        ( 0     ),
  otherScopeLines_( 0     )
{
	buf_.reserve( bufsz );
	setg( &buf_[0], &buf_[0], &buf_[0] );
	current_ = streams_.end();
}

int
StreamMuxBuf::nlCount(const Char *beg, const Char *endp)
{
int         rval = 0;
	while ( beg != endp ) {
		if ( '\n' == *beg )
			rval++;
		++beg;
	}
	return rval;
}

unsigned
StreamMuxBuf::getOtherScopeLines()
{
	return otherScopeLines_ - current_->headerLines;
}

const char *
StreamMuxBuf::getFileName()
{
	return current_->name.c_str();
}

int
StreamMuxBuf::underflow()
{
bool counted = false;

	while ( gptr() == egptr() ) {
		std::streamsize got;

		if ( current_ == streams_.end() ) {
			done_ = true;
			return std::char_traits<Char>::eof();
		}
		if ( ! counted ) {
			nlSoFar_ += nlCount( &buf_[0], egptr() );
			counted = true;
		}
		got = (*current_).stream->rdbuf()->sgetn( &buf_[0], bufsz_);
		if ( got > 0 ) {
			setg( &buf_[0], &buf_[0], &buf_[0] + got );
		} else {
			++current_;
			otherScopeLines_ += nlSoFar_;
			nlSoFar_ = 0;
		}
	}
	return std::char_traits<Char>::to_int_type( *gptr() );
}

void
StreamMuxBuf::pushbuf(Stream s, const std::string *name, unsigned headerLines)
{
	if ( done_ )
		throw StreamDoneError("Cannot push new streams to StreamMuxBuf with EOF status");

	if ( s->fail() ) {
		FailedStreamError e("StreamMuxBuf::pushbuf -- cannot push failed stream; in: ");
		e.append( *name );
		throw e;
	}


	bool first_el(streams_.empty());

	streams_.push_back( StreamEl( s, name, headerLines ) );

	if ( first_el )
		current_ = streams_.begin();
}

void
StreamMuxBuf::dump( std::ostream &os )
{
	std::istream ins( this );
	while ( ins.good() ) {
		char ch;

		ins.get(ch);
		os.put(ch);
	}
}

StreamMuxBuf::Stream
StreamMuxBuf::mkstrm(const char *fnam)
{
std::ifstream *p = new std::ifstream( fnam );

	if ( p->fail() ) {
		delete p;
		FailedStreamError e("Creation of stream for'");
		e.append(fnam);
		e.append("' failed");

		throw e;
	}

	return Stream( p );
}

StreamMuxBuf::Stream
StreamMuxBuf::mkstrm(const std::string &contents)
{
std::stringstream *p = new std::stringstream( contents );
	if ( p->fail() ) {
		delete p;
		FailedStreamError e("Creation of stringstream failed");
		throw e;
	}
	return Stream( p );
}

YamlPreprocessor::YamlPreprocessor(StreamMuxBuf::Stream inp, StreamMuxBuf *mux, const char *yaml_dir)
: paths_     ( getenv( YAML_PATH ) ),
  mainName_  ( "<stream>"          ),
  main_      ( inp                 ),
  mux_       ( mux                 ),
  versionSet_( false               ),
  major_     ( -1                  ),
  minor_     ( -1                  ),
  revision_  ( -1                  ),
  verbose_   ( false               )
{
	if ( inp->fail() ) {
		throw FailedStreamError("YamlPreprocessor: main stream has 'failed' status");
	}
	if ( ! yaml_dir && ! getenv( YAML_PATH ) ) {
		yaml_dir = "./";
	}
	paths_.appendPath( yaml_dir );
	// search for relative include files, e.g., include/blah.yaml
	// in YAML_PATH
	paths_.setTryAllNames( true );
}

static std::string
buildMainName(const char *nm)
{
std::string rval( nm );
	// if YAML_PATH is unset and the name contains no '/' then prepend './'
	if ( ! strchr( nm, '/' ) && ! getenv( YAML_PATH ) ) {
		return std::string( "./" ) + rval;
	}
	return rval;
}

YamlPreprocessor::YamlPreprocessor(const char *main_name, StreamMuxBuf *mux, const char *yaml_dir)
: paths_     ( getenv( YAML_PATH )                         ),
  mainName_  ( paths_.lookup( buildMainName( main_name ) ) ),
  main_      ( StreamMuxBuf::mkstrm( mainName_.c_str() )   ),
  mux_       ( mux                                         ),
  versionSet_( false                                       ),
  major_     ( -1                                          ),
  minor_     ( -1                                          ),
  revision_  ( -1                                          ),
  verbose_   ( false                                       )
{
const char  *sep;
std::string  main_dir;
	if ( ! yaml_dir ) {
		if ( (sep = ::strrchr(main_name,'/')) ) {
			main_dir = std::string(main_name);
			main_dir.resize(sep - main_name);
			yaml_dir = main_dir.c_str();
		} else {
			// Traditionally, the search path for files was './' if yaml_dir
			// was empty. Switch this behavior off if YAML_PATH is set (user
			// has to provide explicit settings).
			if ( ! getenv( YAML_PATH ) ) {
				yaml_dir = "./";
			}
		}
	}
	paths_.appendPath( yaml_dir );
	// search for relative include files, e.g., include/blah.yaml
	// in YAML_PATH
	paths_.setTryAllNames( true );
}

bool
YamlPreprocessor::check_exists(const std::string &key)
{
std::pair< Map::iterator, bool > ret = tags_.insert( key );
	return ! ret.second;
}

// pre c++11 doesn't have to_string :-(
static std::string
tostr(int n)
{
char hlbuf[30];
	snprintf(hlbuf, sizeof(hlbuf), "%d", n);
	return std::string( hlbuf );
}

void
YamlPreprocessor::process(StreamMuxBuf::Stream current, const std::string &name)
{
int maj = -1;
int min = -1;
int rev = -1;

unsigned headerLines = 0;

	while ( '#' == current->peek() ) {
		std::string line;
		std::getline(*current, line );
		bool onceHasNoTag;

		std::string hl( tostr( headerLines ) );

		if ( verbose_ ) {
			mux_->pushbuf( StreamMuxBuf::mkstrm( std::string("##(") + hl + std::string(") ") + line + std::string("\n") ), &name, headerLines );
		}

		headerLines++;

		if ( 0 == line.compare(1, 4, "once")
             && ( (onceHasNoTag = (line.size() < 6)) || (line[5] == ' ')  ) ) {

			size_t beg = line.find_first_not_of(" \t", 5);
			size_t len;

			if ( std::string::npos == beg ) {
				onceHasNoTag = true;
			}

			if ( onceHasNoTag ) {
				CPSW::sErr() << "WARNING: (YamlPreprocessor) #once line lacks a 'tag' -- in : " + name + " (will use filename for now but future version may escalate to ERROR) \n";
//				MissingOnceTagError e("YamlPreprocessor: #once line lacks a 'tag' -- in: ");
//				e.append( name );
//				throw e;
			} else {
				// maybe there is an '\r' (windows-generated file)
				len = line.find_first_of( " \t\r", beg );

				if ( std::string::npos != len )
					len -= beg;
			}

			const std::string &tag = onceHasNoTag ? name : line.substr(beg, len);

			// if tag exist already then ignore rest of file
			if ( check_exists( tag ) )
				return; 

		} else if ( 0 == line.compare(1, 8, "include ") ) {

			size_t beg = line.find_first_not_of(" \t", 8);

			if ( std::string::npos == beg ) {
				MissingIncludeFileNameError e("YamlPreprocessor: #include line lacks a 'filename' -- in: ");
				e.append( name );
				throw e;
			}

			bool   hasOpenAngle = (0 == line.compare(beg, 1, "<"));

			if ( hasOpenAngle )
				beg++;

			// maybe there is an '\r' (windows-generated file)
			size_t len = line.find_first_of( (hasOpenAngle ? ">" : " \t\r"), beg );

			if ( std::string::npos != len )
				len -= beg;

			// recursively process included file

			std::string incFnam( paths_.lookup( line.substr(beg, len).c_str() ) );

			process( StreamMuxBuf::mkstrm( incFnam.c_str() ), incFnam );

		} else if ( 0 == line.compare(1, 14, "schemaversion ") ) {
			if ( 3 != ::sscanf( line.c_str() + 15, "%d.%d.%d", &maj, &min, &rev  ) ) {
				BadSchemaVersionError e("YamlPreprocessor: #schemaversion lacks <major>.<minor>.<revision> triple -- in: "); 
				e.append( name );
				throw e;
			}
			if ( ! versionSet_ ) {
				major_      = maj;
				minor_      = min;
				revision_   = rev;
				versionSet_ = true;
			}
		}
	}

	if ( ! versionSet_ ) {
		// a version has not been set yet and we don't have
		// a schemaversion either. Thus leave the version numbet at -1
		// but declare the version as set
		versionSet_ = true;
	}

	if ( major_ != maj ) {
		BadSchemaVersionError e("YamlPreprocessor: mismatch of major versions among files -- in: ");
		e.append( name );
		throw e;
	} else if ( min < minor_ ) {
		minor_    = min;
		revision_ = rev;
	} else if ( min == minor_ && rev < revision_ ) {
		revision_ = rev;
	}

	if ( current->good() ) {
		mux_->pushbuf( current, &name, headerLines );
	} else if ( current->fail() ) {
		FailedStreamError e("YamlPreprocessor: stream has 'failed' status after processing header -- in: ");
		e.append( name );
		throw e;
	}
	// skip this stream if EOF
}

void
YamlPreprocessor::process()
{
	process( main_, mainName_ );
}

