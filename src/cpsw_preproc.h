 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_YAML_PREPROCESSOR_H
#define CPSW_YAML_PREPROCESSOR_H

#include <streambuf>
#include <istream>
#include <vector>
#include <list>
#include <string>

#include <cpsw_compat.h>
#include <cpsw_flookup.h>
#include <cpsw_error.h>

using std::streambuf;

// class to multiplex/concatenate multiple
// istreams into a single streambuf.
// note that while intermediate copying is performed
// (into the internal buffer) the individual streams
// are not 'slurped' into a huge buffer space.


class StreamMuxBuf : public std::streambuf {
public:
	typedef shared_ptr<std::istream> Stream;
private:
	struct                      StreamEl {
		Stream       stream;
		std::string  name;
		unsigned     headerLines;

		StreamEl(Stream, const std::string*, unsigned);
	};
	typedef char                Char;
	typedef std::list<StreamEl> StreamsContainer;

	StreamsContainer            streams_;
	StreamsContainer::iterator  current_;
	std::vector<Char>           buf_;
	unsigned                    bufsz_;
	bool                        done_;
	unsigned                    nlSoFar_;
	unsigned                    otherScopeLines_;

	static int                  nlCount(const Char *, const Char *);

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
	virtual void pushbuf(Stream s, const std::string *name, unsigned headerLines);

	// get the number of lines processed that are *not* in the
	// current file (to be subtracted from the yaml parser line number)
	virtual unsigned  getOtherScopeLines();

	// get the name of the current file
	virtual const char *getFileName();

	// for testing; dump concatenated streams to cout
	virtual void dump();

	// create a Stream; throws FailedStreamException e.g., if file is not found
	static Stream mkstrm(const char            *fnam);
	static Stream mkstrm(const std::string &contents);
};

class YamlPreprocessor {
private:
	typedef cpsw::unordered_set<std::string>  Map;
	FLookup                                   paths_;
	std::string                               mainName_;
	StreamMuxBuf::Stream                      main_;
	StreamMuxBuf                             *mux_;
	Map                                       tags_;
	bool                                      versionSet_;
	int                                       major_;
	int                                       minor_;
	int                                       revision_;
	bool                                      verbose_;

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
	YamlPreprocessor(StreamMuxBuf::Stream inp, StreamMuxBuf *mux, const char *yaml_dir);

	int getSchemaVersionMajor() const
	{
		return major_;
	}

	int getSchemaVersionMinor() const
	{
		return minor_;
	}

	int getSchemaVersionRevision() const
	{
		return revision_;
	}

	bool getVerbose() const
	{
		return verbose_;
	}

	void setVerbose(bool verbose)
	{
		verbose_ = verbose;
	}

	// helper which creates the main Stream
	YamlPreprocessor(const char *main_name, StreamMuxBuf *mux, const char *yaml_dir);

protected:
	// check if a given 'once' tag exists and record it otherwise
	// RETURNS: true if already present.
	virtual bool check_exists(const std::string &key);

	// process a Stream
	virtual void process(StreamMuxBuf::Stream current, const std::string &name);

public:

	// process the main stream, recursively processing
	// included files
	virtual void process();
};
#endif
