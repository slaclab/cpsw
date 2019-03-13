#ifndef CPSW_COMPAT_H
#define CPSW_COMPAT_H

#include <unistd.h>

#include <cpsw_shared_ptr.h>

#ifdef CPSW_USES_BOOST
#include <boost/unordered_set.hpp>
#include <boost/atomic.hpp>

namespace cpsw {
  using boost::shared_ptr;
  using boost::unordered_set;
  using boost::atomic;
  using boost::memory_order_relaxed;
  using boost::memory_order_acq_rel;
  using boost::memory_order_acquire;
  using boost::memory_order_release;
};
#endif

#ifdef CPSW_USES_STD

#include <unordered_set>
#include <atomic>

namespace cpsw {
  using std::shared_ptr;
  using std::unordered_set;
  using std::atomic;
  using std::memory_order_relaxed;
  using std::memory_order_acq_rel;
  using std::memory_order_acquire;
  using std::memory_order_release;
};
#endif

#endif
