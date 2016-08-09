
#include <cpsw_preproc.h>

#include <iostream>
#include <fstream>

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

YamlPreprocessor::YamlPreprocessor(StreamMuxBuf::Stream inp, StreamMuxBuf *mux)
: main_(inp),
  mux_(mux)
{
	if ( inp->fail() ) {
		throw FailedStreamError("YamlPreprocessor: main stream has 'failed' status");
	}
}

YamlPreprocessor::YamlPreprocessor(const char *main_name, StreamMuxBuf *mux)
: main_( StreamMuxBuf::mkstrm( main_name ) ),
  mux_(mux)
{
	std::string full_path( main_name );
	int idx = full_path.find_last_of("\\/");
	path_  = full_path.substr(0, idx + 1); // get path including "/"
}

bool
YamlPreprocessor::check_exists(const std::string &key)
{
std::pair< Map::iterator, bool > ret = tags_.insert( key );
	return ! ret.second;
}

void
YamlPreprocessor::process(StreamMuxBuf::Stream current)
{
	while ( '#' == current->peek() ) {
		std::string line;
		std::getline(*current, line );

		if ( 0 == line.compare(1, 5, "once ") ) {
			if ( line.size() < 7 )
				throw MissingOnceTagError("YamlPreprocessor: #once line lacks a 'tag'");

			// if tag exist already then ignore rest of file
			if ( check_exists( line.substr(6) ) )
				return; 

		} else if ( 0 == line.compare(1, 8, "include ") ) {
			size_t beg = line.find_first_not_of(" \t", 8);
			if ( beg == std::string::npos ) {
				throw MissingIncludeFileNameError("YamlPreprocessor: #include line lacks a 'filename'");
			}
			size_t len = line.find_first_of(" \t",beg);

			if ( ! std::string::npos != len )
				len -= beg;

			// recursively process included file
			// in same directory
			process( StreamMuxBuf::mkstrm( ( path_ + line.substr(beg, len) ).c_str() ) );
		}
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
	process( main_ );
}
