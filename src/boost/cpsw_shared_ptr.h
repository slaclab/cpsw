#ifndef CPSW_SHARED_PTR_IMPORT_H
#define CPSW_SHARED_PTR_IMPORT_H

#define CPSW_USES_BOOST 1
#undef  CPSW_USES_STD

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/move/unique_ptr.hpp>

using boost::shared_ptr;

namespace cpsw {
	using boost::weak_ptr;
	using boost::make_shared;
	using boost::allocate_shared;
	using boost::dynamic_pointer_cast;
	using boost::static_pointer_cast;
	using boost::movelib::unique_ptr;
	using boost::move;
};

#endif
