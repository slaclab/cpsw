#include <streambuf>
#include <istream>
#include <vector>
#include <iostream>
#include <fstream>
#include <stack>
#include <map>

#include <yaml-cpp/yaml.h>

using std::vector;
using std::streambuf;

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/unordered_set.hpp>

using boost::shared_ptr;
using boost::make_shared;
using boost::unordered_set;

class StreamDoneException     {};
class FailedStreamException   {};
class MissingOnceTagException {};
class MissingIncludeFileNameException{};

typedef shared_ptr<std::istream> Stream;

class StreamMuxBuf : public std::streambuf {
private:
	typedef char Char;
	typedef vector<Stream> Vec;
	Vec               streams_;
	Vec::iterator     current_;
	Char             *buf_;
	unsigned          bufsz_;
	bool              done_;

	StreamMuxBuf(const StreamMuxBuf &);
	StreamMuxBuf operator=(const StreamMuxBuf&);

public:

	StreamMuxBuf(unsigned bufsz = 1024)
	: buf_( new Char[bufsz] ),
	  bufsz_(bufsz),
	  done_(false)
	{
		streams_.reserve(10);
		current_ = streams_.begin();
	}

	virtual int underflow()
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

	virtual void pushbuf(Stream s)
	{
		if ( done_ )
			throw StreamDoneException();

		if ( s->fail() )
			throw FailedStreamException();

		streams_.push_back( s );
	}

	// for testing
	virtual void dump()
	{
	std::istream ins( this );
		while ( ins.good() ) {
			char ch;

			ins.get(ch);
			std::cout.put(ch);
		}
	}

};

Stream mkstrm(const char *fnam)
{
std::ifstream *p = new std::ifstream( fnam );

	if ( p->fail() ) {
		delete p;
		throw FailedStreamException();
	}

	return Stream( p );
}

class Preprocessor {
private:
	typedef unordered_set<std::string>  Map;
	Stream                              main_;
	std::stack<Stream>                  stack_;
	StreamMuxBuf                       *mux_;
	Map                                 tags_;

	Preprocessor(const Preprocessor &);
	Preprocessor operator=(const Preprocessor&);

public:
	Preprocessor(Stream inp, StreamMuxBuf *mux)
	: main_(inp),
	  mux_(mux)
	{
		if ( inp->fail() ) {
			throw FailedStreamException();
		}
	}

	Preprocessor(const char *main_name, StreamMuxBuf *mux)
	: main_( mkstrm( main_name ) ),
	  mux_(mux)
	{
	}

	bool
	check_exists(const std::string &key)
	{
	std::pair< Map::iterator, bool > ret = tags_.insert( key );
		return ! ret.second;
	}

	virtual void process(Stream current)
	{
		while ( '#' == current->peek() ) {
			std::string line;
			std::getline(*current, line );

			if ( 0 == line.compare(1, 5, "once ") ) {
				if ( line.size() < 7 )
					throw MissingOnceTagException();
				if ( check_exists( line.substr(6) ) )
					return; // ignore rest of file
			} else if ( 0 == line.compare(1, 8, "include ") ) {
				size_t beg = line.find_first_not_of(" \t", 8);
				if ( beg == std::string::npos ) {
					throw MissingIncludeFileNameException();
				}
				size_t len = line.find_first_of(" \t",beg);

				if ( ! std::string::npos != len )
					len -= beg;

				process( mkstrm( line.substr(beg, len).c_str() ) );
			}
		}

		if ( current->good() ) {
			mux_->pushbuf( current );
		} else if ( current->fail() ) {
			throw FailedStreamException();
		}
		// skip this stream if EOF
	}

	virtual void process()
	{
		process( main_ );
	}

};

int
main(int argc, char **argv)
{
StreamMuxBuf   dut;
std::istream   duts( &dut );
Preprocessor   proc( "f1.txt", &dut );


	proc.process();

	YAML::Emitter emit(std::cout);

#if 0
	emit << YAML::Load(duts);

	std::cout << "\n";
#endif

	dut.dump();

}
