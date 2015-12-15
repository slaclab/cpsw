#ifndef CPSW_ERROR_H
#define CPSW_ERROR_H

#include <string>

using std::string;

class CPSWError {
	private:
		string name_;
	public:
		CPSWError(const string &s):name_(s) {}
		CPSWError(const char *n):name_(n)  {}
		CPSWError(const Path p):name_("FIXME") {}

		virtual string &getInfo() { return name_; }

};

class DuplicateNameError : public CPSWError {
	public:
		DuplicateNameError(const char *n) : CPSWError(n) {}
};

class NotDevError : public CPSWError {
	public:
		NotDevError(const char *n) : CPSWError(n) {}
		NotDevError(const Path  p) : CPSWError(p) {}
};

class NotFoundError : public CPSWError {
	public:
		NotFoundError(const char *n) : CPSWError(n) {}
};

class InvalidPathError : public CPSWError {
	public:
		InvalidPathError(const char *n) : CPSWError(n) {}
		InvalidPathError(const Path  p) : CPSWError(p) {}
};

class InvalidIdentError: public CPSWError {
	public:
		InvalidIdentError(const char   *n) : CPSWError(n) {}
		InvalidIdentError(const string &n) : CPSWError(n) {}
};

class InvalidArgError: public CPSWError {
	public:
		InvalidArgError(const char *n) : CPSWError(n)   {}
};

class AddressAlreadyAttachedError: public CPSWError {
	public:
		AddressAlreadyAttachedError(const char *n) : CPSWError(n)   {}
};

class ConfigurationError: public CPSWError {
	public:
		ConfigurationError(const char *s) : CPSWError(s) {}
};


class InternalError: public CPSWError {
	public:
		InternalError() : CPSWError("Internal Error") {}
		InternalError(const char*s) : CPSWError(s) {}
};

class AddrOutOfRangeError: public CPSWError {
	public:
		AddrOutOfRangeError(const char *s) : CPSWError(s) {}
};

class InterfaceNotImplementedError: public CPSWError {
	public:
		InterfaceNotImplementedError( Path p ) : CPSWError( p ) {}
};

class IOError: public CPSWError {
	public:
		IOError( const char *s ) : CPSWError( s ) {}
		IOError( string &s )     : CPSWError( s ) {}
		IOError( const char *s, int err) : CPSWError( string(s).append(strerror(err)) ) {}
};

class BadStatusError: public CPSWError {
	protected:
		int64_t status_;
	public:
		BadStatusError( const char *s, int64_t status) :CPSWError(s), status_(status) {}
};

class IntrError : public CPSWError {
	public:
		IntrError(const char *s) : CPSWError(s) {}
};

#endif
