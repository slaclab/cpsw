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
#include <stdio.h>
#include <exception>
#include <cpsw_shared_ptr.h>


/* Use shared pointers if we have to hand error objects around */
class CPSWError;
typedef shared_ptr<CPSWError> CPSWErrorHdl;

class CPSWError : public std::exception {
private:
	std::string name_;
	std::string type_;
	std::string msg_;
protected:
	void
	buildMsg()
	{
		msg_ = std::string("CPSW ") + type_ + std::string(": ") + name_;
	}

	template <typename T>
	CPSWError(const T s, const char *typeName)
	: name_( s        ),
	  type_( typeName )
	{
		buildMsg();
	}

public:
	virtual const char *typeName() const { return "Error"; }

	CPSWError(const std::string & s)
	: name_( s ),
	  type_( typeName() )
	{
		buildMsg();
	}

	CPSWError(const char *s)
	: name_( s ),
	  type_( typeName() )
	{
		buildMsg();
	}

	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }

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
		return msg_;
	}

	virtual const char *what() const throw()
	{
		return getInfo().c_str();
	}


	virtual ~CPSWError() throw()
	{
	}
};

class NoError: public CPSWError {
public:
	NoError()
	: CPSWError( "No Error", typeName() )
	{
	}

	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }

	virtual const char *typeName() const { return "NoError"; }

};

class DuplicateNameError: public CPSWError {
public:
	DuplicateNameError(const char *n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }

	virtual const char *typeName() const { return "DuplicateNameError"; }
};

class NotDevError: public CPSWError {
public:
	NotDevError(const char *n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }

	virtual const char *typeName() const { return "NotDevError"; }
};

class NotFoundError: public CPSWError {
public:
	NotFoundError(const char *n)
	: CPSWError( n, typeName() )
	{
	}

	NotFoundError(const std::string &n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "NotFoundError"; }
};

class InvalidPathError: public CPSWError {
public:
	InvalidPathError(const std::string &n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "InvalidPathError"; }
};

class InvalidIdentError: public CPSWError {
public:
	InvalidIdentError(const char   *n)
	: CPSWError( n, typeName() )
	{
	}

	InvalidIdentError(const std::string &n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "InvalidIdentError"; }
};

class InvalidArgError: public CPSWError {
public:
	InvalidArgError(const char *n)
	: CPSWError( n, typeName() )
	{
	}

	InvalidArgError(const std::string &n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "InvalidArgError"; }
};

class AddressAlreadyAttachedError: public CPSWError {
public:
	AddressAlreadyAttachedError(const char *n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "AddressAlreadyAttachedError"; }
};

class ConfigurationError: public CPSWError {
public:
	ConfigurationError(const char *n)
	: CPSWError( n, typeName() )
	{
	}

	ConfigurationError(const std::string &n)
	: CPSWError( n, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "ConfigurationError"; }
};

class ErrnoError: public CPSWError {
protected:
	template <typename T>
	static std::string buildString(T s, int err = errno)
	{
		return std::string( s ).append(": ").append( strerror( err ) );
	}

	template <typename T>
	ErrnoError(const T s, const char *typeName)
	: CPSWError( buildString(s) ),
	  err_(errno)
	{
	}

	template <typename T, typename U>
	ErrnoError(const T s, const U err, const char *typeName)
	: CPSWError( buildString(s, err), typeName ),
	  err_( err )
	{
	}



public:
	int err_;

	ErrnoError(const char *n)
	: CPSWError( buildString( n ), typeName() ),
	  err_( errno )
	{
	}

	ErrnoError(const std::string &n)
	: CPSWError( buildString( n ), typeName() ),
	  err_( errno )
	{
	}

	ErrnoError(const std::string &s, int err)
	: CPSWError( buildString( s, err ), typeName() ),
	  err_( err )
	{
	}


	ErrnoError(const char *s, int err)
	: CPSWError( buildString( s, err ), typeName() ),
	  err_( err )
	{
	}

	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "ErrnoError"; }
};


class InternalError: public ErrnoError {
protected:
	void fatal()
	{
		fprintf(stderr,"%s\n", what());
		fprintf(stderr,"ABORTING (so that the core-dump gets a stack trace from where this was thrown...)\n");
	}
public:
	InternalError()
	: ErrnoError("Internal Error", typeName())
	{
		fatal();
	}

	InternalError(const char*s)
	: ErrnoError(s, typeName())
	{
		fatal();
	}

	InternalError(const std::string &s)
	: ErrnoError(s, typeName())
	{
		fatal();
	}

	InternalError(const std::string &s, int err)
	: ErrnoError(s, err, typeName())
	{
		fatal();
	}


	InternalError(const char*s, int err)
	: ErrnoError(s, err, typeName())
	{
		fatal();
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "InternalError"; }
};

class AddrOutOfRangeError: public CPSWError {
public:
	AddrOutOfRangeError(const char *s)
	: CPSWError(s, typeName())
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "AddrOutOfRangeError"; }
};

class ConversionError: public CPSWError {
public:
	ConversionError(const char *s)
	: CPSWError(s, typeName())
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "ConversionError"; }
};


class InterfaceNotImplementedError: public CPSWError {
public:
	InterfaceNotImplementedError( const std::string &s )
	: CPSWError(s, typeName())
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "InterfaceNotImplementedError"; }
};

class IOError: public ErrnoError {
public:
	IOError( const char *s )
	: ErrnoError( s, typeName() )
	{
	}

	IOError( std::string &s )
	: ErrnoError( s, typeName() )
	{
	}

	IOError( std::string &s, int err )
	: ErrnoError( s, err, typeName() )
	{
	}


	IOError( const char *s, int err)
	: ErrnoError( s, err, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "IOError"; }
};

class BadStatusError: public CPSWError {
protected:
	int64_t status_;
public:
	BadStatusError( const char *s, int64_t status)
	: CPSWError( s, typeName() ),
	  status_(status)
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "BadStatusError"; }
};

class IntrError: public CPSWError {
public:
	IntrError(const char *s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "IntrError"; }
};

class StreamDoneError : public CPSWError {
public:
	StreamDoneError(const char *s)
	: CPSWError( s, typeName() )
	{
	}

	StreamDoneError(const std::string &s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "StreamDoneError"; }
};

class FailedStreamError : public CPSWError {
public:
	FailedStreamError(const char* s)
	: CPSWError( s, typeName() )
	{
	}

	FailedStreamError(const std::string &s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "FailedStreamError"; }
};

class MissingOnceTagError : public CPSWError {
public:
	MissingOnceTagError(const char *s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "MissingOnceTagError"; }
};

class MissingIncludeFileNameError : public CPSWError {
public:
	MissingIncludeFileNameError(const char *s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "MissingIncludeFileNameError"; }
};

class NoYAMLSupportError : public CPSWError {
public:
	NoYAMLSupportError()
	: CPSWError( "No YAML support compiled in", typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "NoYAMLSupportError"; }
};

class MultipleInstantiationError : public CPSWError {
public:
	MultipleInstantiationError(const char *s)
	: CPSWError( s, typeName() )
	{
	}

	MultipleInstantiationError(const std::string &s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "MultipleInstantiationError"; }
};

class BadSchemaVersionError : public CPSWError {
public:
	BadSchemaVersionError(const char *s)
	: CPSWError( s, typeName() )
	{
	}

	BadSchemaVersionError(const std::string &s)
	: CPSWError( s, typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "BadSchemaVersionError"; }
};

class TimeoutError : public CPSWError {
public:
	template <typename T>
	TimeoutError(const T s)
	: CPSWError( s, typeName() )
	{
	}

	TimeoutError()
	: CPSWError( "Timeout", typeName() )
	{
	}
	// every subclass MUST implement 'clone'
	virtual CPSWErrorHdl clone() { return cpsw::make_shared<CPSWError>(*this); }
	// every subclass MUST implement 'throwMe'
	virtual void throwMe() { throw *this; }
	virtual const char *typeName() const { return "TimeoutError"; }
};

//!!!!!!!!!!!!
//
// When added new exceptions -- don't forget to register them with
// Python (cpsw_python.cc -- ExceptionTranslatorInstall)
//
//!!!!!!!!!!!!


#endif
