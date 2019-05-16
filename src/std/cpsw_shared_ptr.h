#ifndef CPSW_SHARED_PTR_IMPORT_H
#define CPSW_SHARED_PTR_IMPORT_H

#undef  CPSW_USES_BOOST
#define CPSW_USES_STD   1

#include <memory>

using std::shared_ptr;

namespace cpsw {
	using std::shared_ptr;
	using std::weak_ptr;
	using std::make_shared;
	using std::allocate_shared;
	using std::dynamic_pointer_cast;
	using std::static_pointer_cast;
	using std::unique_ptr;
	using std::move;
};

#endif
