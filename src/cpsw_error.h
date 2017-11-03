 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_ERROR_H
#define CPSW_ERROR_H

#include <string>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <exception>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

using boost::shared_ptr;
using boost::make_shared;

/* Use shared pointers if we have to hand error objects around */
class CPSWError;
typedef shared_ptr<CPSWError> CPSWErrorHdl;

class CPSWError : public std::exception {
private:
	std::string name_;
public:
	CPSWError(const std::string &s):name_(s)
	{
	}

	CPSWError(const char *n):name_(n)
	{
	}

	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }

	virtual void
	prepend(const std::string &s)
	{
		name_.insert(0, s);
	}

	virtual void
	append(const std::string &s)
	{
		name_.append(s);
	}

	virtual const std::string &getInfo() const
	{
		return name_;
	}

	virtual const char *what() const throw()
	{
		return getInfo().c_str();
	}

	virtual std::string typeName() const { return std::string("Error"); }

	virtual ~CPSWError() throw()
	{
	}
};

class NoError: public CPSWError {
public:
	NoError()
	: CPSWError("No Error")
	{
	}

	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }

	virtual std::string typeName() const { return std::string("NoError"); }

};

class DuplicateNameError: public CPSWError {
public:
	DuplicateNameError(const char *n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }

	virtual std::string typeName() const { return std::string("DuplicateNameError"); }
};

class NotDevError: public CPSWError {
public:
	NotDevError(const char *n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }

	virtual std::string typeName() const { return std::string("NotDevError"); }
};

class NotFoundError: public CPSWError {
public:
	NotFoundError(const char *n)
	: CPSWError(n)
	{
	}

	NotFoundError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("NotFoundError"); }
};

class InvalidPathError: public CPSWError {
public:
	InvalidPathError(const std::string &n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("InvalidPathError"); }
};

class InvalidIdentError: public CPSWError {
public:
	InvalidIdentError(const char   *n)
	: CPSWError(n)
	{
	}

	InvalidIdentError(const std::string &n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("InvalidIdentError"); }
};

class InvalidArgError: public CPSWError {
public:
	InvalidArgError(const char *n)
	: CPSWError(n)
	{
	}

	InvalidArgError(const std::string &n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("InvalidArgError"); }
};

class AddressAlreadyAttachedError: public CPSWError {
public:
	AddressAlreadyAttachedError(const char *n)
	: CPSWError(n)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("AddressAlreadyAttachedError"); }
};

class ConfigurationError: public CPSWError {
public:
	ConfigurationError(const char *s)
	: CPSWError(s)
	{
	}

	ConfigurationError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("ConfigurationError"); }
};

class ErrnoError: public CPSWError {
public:
	int err_;
	ErrnoError(const char *s)
	: CPSWError( s )
	{
	}

	ErrnoError(const std::string &s)
	: CPSWError( s ),
	  err_(0)
	{
	}

	ErrnoError(const std::string &s, int err)
	: CPSWError( (s + std::string(": ")).append(strerror(err)) ),
	  err_( err )
	{
	}


	ErrnoError(const char *s, int err)
	: CPSWError( std::string(s).append(": ").append(strerror(err)) ),
	  err_( err )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("ErrnoError"); }
};


class InternalError: public ErrnoError {
public:
	InternalError()
	: ErrnoError("Internal Error")
	{
		abort(); // so core-dump gets stack trace from where this was thrown
	}

	InternalError(const char*s)
	: ErrnoError(s)
	{
		abort(); // so core-dump gets stack trace from where this was thrown
	}

	InternalError(const std::string &s)
	: ErrnoError(s)
	{
		abort(); // so core-dump gets stack trace from where this was thrown
	}

	InternalError(const std::string &s, int err)
	: ErrnoError(s, err)
	{
		abort(); // so core-dump gets stack trace from where this was thrown
	}


	InternalError(const char*s, int err)
	: ErrnoError(s, err)
	{
		abort(); // so core-dump gets stack trace from where this was thrown
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("InternalError"); }
};

class AddrOutOfRangeError: public CPSWError {
public:
	AddrOutOfRangeError(const char *s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("AddrOutOfRangeError"); }
};

class ConversionError: public CPSWError {
public:
	ConversionError(const char *s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("ConversionError"); }
};


class InterfaceNotImplementedError: public CPSWError {
public:
	InterfaceNotImplementedError( const std::string &s )
	: CPSWError( s )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("InterfaceNotImplementedError"); }
};

class IOError: public ErrnoError {
public:
	IOError( const char *s )
	: ErrnoError( s )
	{
	}

	IOError( std::string &s )
	: ErrnoError( s )
	{
	}

	IOError( std::string &s, int err )
	: ErrnoError( s, err )
	{
	}


	IOError( const char *s, int err)
	: ErrnoError( s, err )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("IOError"); }
};

class BadStatusError: public CPSWError {
protected:
	int64_t status_;
public:
	BadStatusError( const char *s, int64_t status)
	:CPSWError( s ),
	 status_(status)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("BadStatusError"); }
};

class IntrError: public CPSWError {
public:
	IntrError(const char *s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("IntrError"); }
};

class StreamDoneError : public CPSWError {
public:
	StreamDoneError(const char *s)
	: CPSWError(s)
	{
	}

	StreamDoneError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("StreamDoneError"); }
};

class FailedStreamError : public CPSWError {
public:
	FailedStreamError(const char* s)
	: CPSWError(s)
	{
	}

	FailedStreamError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("FailedStreamError"); }
};

class MissingOnceTagError : public CPSWError {
public:
	MissingOnceTagError(const char *s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("MissingOnceTagError"); }
};

class MissingIncludeFileNameError : public CPSWError {
public:
	MissingIncludeFileNameError(const char *s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("MissingIncludeFileNameError"); }
};

class NoYAMLSupportError : public CPSWError {
public:
	NoYAMLSupportError()
	: CPSWError("No YAML support compiled in")
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("NoYAMLSupportError"); }
};

class MultipleInstantiationError : public CPSWError {
public:
	MultipleInstantiationError(const char *s)
	: CPSWError(s)
	{
	}

	MultipleInstantiationError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("MultipleInstantiationError"); }
};

class BadSchemaVersionError : public CPSWError {
public:
	BadSchemaVersionError(const char *s)
	: CPSWError(s)
	{
	}

	BadSchemaVersionError(const std::string &s)
	: CPSWError(s)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("BadSchemaVersionError"); }
};

class TimeoutError : public CPSWError {
public:
	TimeoutError(const char *s)
	: CPSWError(s)
	{
	}

	TimeoutError(const std::string &s)
	: CPSWError(s)
	{
	}

	TimeoutError()
	: CPSWError("Timeout")
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return make_shared<CPSWError>(*this); }
	virtual std::string typeName() const { return std::string("TimeoutError"); }
};

//!!!!!!!!!!!!
//
// When added new exceptions -- don't forget to register them with
// Python (cpsw_python.cc -- ExceptionTranslatorInstall)
//
//!!!!!!!!!!!!


#endif
