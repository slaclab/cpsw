#ifndef CPSW_ERROR_H
#define CPSW_ERROR_H

#include <string>

class CPSWError {
	private:
		std::string name;
	public:
		CPSWError(std::string &s):name(s) {}
		CPSWError(const char *n):name(n)  {}
		CPSWError(const Path p):name("FIXME") {}

		virtual std::string &getInfo() { return name; }
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
		InvalidIdentError(const char *n) : CPSWError(n) {}
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

#endif
