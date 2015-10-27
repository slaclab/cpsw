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


class InternalError: public CPSWError {
	public:
		InternalError() : CPSWError("Internal Error") {}
};

#endif
