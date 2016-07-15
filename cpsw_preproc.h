#ifndef CPSW_YAML_PREPROCESSOR_H
#define CPSW_YAML_PREPROCESSOR_H

#include <streambuf>
#include <istream>
#include <vector>

#include <cpsw_error.h>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_set.hpp>

using std::streambuf;

using boost::shared_ptr;
using boost::unordered_set;

// class to multiplex/concatenate multiple
// istreams into a single streambuf.
// note that while intermediate copying is performed
// (into the internal buffer) the individual streams
// are not 'slurped' into a huge buffer space.

class StreamMuxBuf : public std::streambuf {
public:
	typedef shared_ptr<std::istream> Stream;
private:
	typedef char                Char;
	typedef std::vector<Stream> Vec;

	Vec                         streams_;
	Vec::iterator               current_;
	Char                       *buf_;
	unsigned                    bufsz_;
	bool                        done_;

	// no copying
	StreamMuxBuf(const StreamMuxBuf &);
	StreamMuxBuf operator=(const StreamMuxBuf&);

public:

	StreamMuxBuf(unsigned bufsz = 1024);

	// override streambuf method;
	virtual int underflow();

	// append a new stream from which to mux
	// they are processed in the order they
	// were pushed.

	// throws: StreamDoneError   - if all pushed streams have already been consumed
	//         FailedStreamError - if the passed stream has the 'fail' bit set
	virtual void pushbuf(Stream s);

	// for testing; dump concatenated streams to cout
	virtual void dump();

	// create a Stream; throws FailedStreamException e.g., if file is not found
	static Stream mkstrm(const char *fnam);
};

class YamlPreprocessor {
private:
	typedef unordered_set<std::string>  Map;
	StreamMuxBuf::Stream                main_;
	StreamMuxBuf                       *mux_;
	Map                                 tags_;

	// no copying
	YamlPreprocessor(const YamlPreprocessor &);
	YamlPreprocessor operator=(const YamlPreprocessor&);

public:
	// constructor
	// 'inp': stream of the 'main' YAML file
	// 'mux': pointer to a muxed stream buffer object

	// after preprocessing a new istream can be
	// created on 'mux':
	//
	//   StramMuxBuf      muxbuf;
	//
	//   YamlPreprocessor preproc( main_input_stream, &muxbuf );
	//
	//                    preproc.process();
	//
	//   std::istream     preprocessed_stream( &muxbuf );
	//
	//   const YAML::Node &node( YAML::Load( preprocessed_stream ) );             
	//
	YamlPreprocessor(StreamMuxBuf::Stream inp, StreamMuxBuf *mux);

	
	// helper which creates the main Stream
	YamlPreprocessor(const char *main_name, StreamMuxBuf *mux);

protected:
	// check if a given 'once' tag exists and record it otherwise
	// RETURNS: true if already present.
	virtual bool check_exists(const std::string &key);

	// process a Stream	
	virtual void process(StreamMuxBuf::Stream current);

public:

	// process the main stream, recursively processing
	// included files
	virtual void process();
};
#endif
