 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.


#include <cpsw_api_user.h>
#include <cpsw_preproc.h>

#include <iostream>
#include <fstream>
#include <string.h>

StreamMuxBuf::StreamMuxBuf(unsigned bufsz)
: buf_( new Char[bufsz] ),
  bufsz_(bufsz),
  done_(false)
{
	current_ = streams_.end();
}

int
StreamMuxBuf::underflow()
{
	while ( gptr() == egptr() ) {
		std::streamsize got;

		if ( current_ == streams_.end() ) {
			done_ = true;
			return std::char_traits<Char>::eof();
		}
		got = (*current_)->rdbuf()->sgetn(buf_, bufsz_);
		if ( got > 0 ) {
			setg( buf_, buf_, buf_ + got );
		} else {
			++current_;
		}
	}
	return std::char_traits<Char>::to_int_type( *gptr() );
}

void
StreamMuxBuf::pushbuf(Stream s)
{
	if ( done_ )
		throw StreamDoneError("Cannot push new streams to StreamMuxBuf with EOF status");

	if ( s->fail() )
		throw FailedStreamError("StreamMuxBuf::pushbuf -- cannot push failed stream");


	bool first_el(streams_.empty());

	streams_.push_back( s );

	if ( first_el )
		current_ = streams_.begin();
}

void
StreamMuxBuf::dump()
{
	std::istream ins( this );
	while ( ins.good() ) {
		char ch;

		ins.get(ch);
		std::cout.put(ch);
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

static void
set_path(std::string *dst, const char *yaml_dir)
{
size_t l = yaml_dir ? strlen(yaml_dir) : 0;
	if ( l > 0 ) {
		std::string sep;
		*dst = std::string(yaml_dir);
		if ( yaml_dir[l-1] != '/' )
			*dst += std::string("/");
	}
}

YamlPreprocessor::YamlPreprocessor(StreamMuxBuf::Stream inp, StreamMuxBuf *mux, const char *yaml_dir)
: main_(inp),
  mainName_("<stream>"),
  mux_(mux),
  versionSet_(false),
  major_(-1),
  minor_(-1),
  revision_(-1)
{
	set_path( &path_, yaml_dir );
	if ( inp->fail() ) {
		throw FailedStreamError("YamlPreprocessor: main stream has 'failed' status");
	}
}

YamlPreprocessor::YamlPreprocessor(const char *main_name, StreamMuxBuf *mux, const char *yaml_dir)
: main_( StreamMuxBuf::mkstrm( main_name ) ),
  mainName_( main_name ),
  mux_(mux),
  versionSet_(false),
  major_(-1),
  minor_(-1),
  revision_(-1)
{
const char  *sep;
std::string  main_dir;
	if ( ! yaml_dir && (sep = ::strrchr(main_name,'/')) ) {
		main_dir = std::string(main_name);
		main_dir.resize(sep - main_name);
		yaml_dir = main_dir.c_str();
	}
	set_path( &path_, yaml_dir );
}

bool
YamlPreprocessor::check_exists(const std::string &key)
{
std::pair< Map::iterator, bool > ret = tags_.insert( key );
	return ! ret.second;
}

void
YamlPreprocessor::process(StreamMuxBuf::Stream current, const std::string &name)
{
int maj = -1;
int min = -1;
int rev = -1;

	while ( '#' == current->peek() ) {
		std::string line;
		std::getline(*current, line );

		if ( 0 == line.compare(1, 5, "once ") ) {

			size_t beg = line.find_first_not_of(" \t", 5);

			if ( std::string::npos == beg ) {
				MissingOnceTagError e("YamlPreprocessor: #once line lacks a 'tag' -- in: ");
				e.append( name );
				throw e;
			}

			// maybe there is an '\r' (windows-generated file)
			size_t len = line.find_first_of( " \t\r", beg );

			if ( std::string::npos != len )
				len -= beg;

			// if tag exist already then ignore rest of file
			if ( check_exists( line.substr(beg, len) ) )
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
			// in same directory
			std::string incFnam( path_ + line.substr(beg, len) );
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
		mux_->pushbuf( current );
	} else if ( current->fail() ) {
		throw FailedStreamError("YamlPreprocessor: stream has 'failed' status after processing header");
	}
	// skip this stream if EOF
}

void
YamlPreprocessor::process()
{
	process( main_, mainName_ );
}
