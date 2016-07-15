#ifndef CPSW_ERROR_H
#define CPSW_ERROR_H

#include <string>
#include <string.h>
#include <stdint.h>

class CPSWError {
private:
	std::string name_;
public:
	CPSWError(const std::string &s):name_(s)
	{
	}

	CPSWError(const char *n):name_(n)
	{
	}

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

	virtual std::string &getInfo()
	{
		return name_;
	}
};

class DuplicateNameError: public CPSWError {
public:
	DuplicateNameError(const char *n)
	: CPSWError(n)
	{
	}
};

class NotDevError: public CPSWError {
public:
	NotDevError(const char *n)
	: CPSWError(n)
	{
	}
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
};

class InvalidPathError: public CPSWError {
public:
	InvalidPathError(const std::string &n)
	: CPSWError(n)
	{
	}
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
};

class AddressAlreadyAttachedError: public CPSWError {
public:
	AddressAlreadyAttachedError(const char *n)
	: CPSWError(n)
	{
	}
};

class ConfigurationError: public CPSWError {
public:
	ConfigurationError(const char *s)
	: CPSWError(s)
	{
	}
};

class ErrnoError: public CPSWError {
public:
	int err_;
	ErrnoError(const char *s)
	: CPSWError( s )
	{
	}

	ErrnoError(const std::string &s)
	: CPSWError( s )
	{
	}


	ErrnoError(const char *s, int err)
	: CPSWError( std::string(s).append(": ").append(strerror(err)) ),
	  err_( err )
	{
	}
};


class InternalError: public ErrnoError {
public:
	InternalError()
	: ErrnoError("Internal Error")
	{
	}

	InternalError(const char*s)
	: ErrnoError(s)
	{
	}

	InternalError(const char*s, int err)
	: ErrnoError(s, err)
	{
	}
};

class AddrOutOfRangeError: public CPSWError {
public:
	AddrOutOfRangeError(const char *s)
	: CPSWError(s)
	{
	}
};

class ConversionError: public CPSWError {
public:
	ConversionError(const char *s)
	: CPSWError(s)
	{
	}
};


class InterfaceNotImplementedError: public CPSWError {
public:
	InterfaceNotImplementedError( const std::string &s )
	: CPSWError( s )
	{
	}
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

	IOError( const char *s, int err)
	: ErrnoError( s, err )
	{
	}
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
};

class IntrError: public CPSWError {
public:
	IntrError(const char *s)
	: CPSWError(s)
	{
	}
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
};

class MissingOnceTagError : public CPSWError {
public:
	MissingOnceTagError(const char *s)
	: CPSWError(s)
	{
	}
};

class MissingIncludeFileNameError : public CPSWError {
public:
	MissingIncludeFileNameError(const char *s)
	: CPSWError(s)
	{
	}
};


#endif
