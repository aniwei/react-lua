#include "react-reconciler/ReactFiberThenable.h"

#include <iostream>

namespace react {
namespace {

class NoopSuspenseyCommitThenable final : public Wakeable {
 public:
  void then(
      std::function<void()> onFulfilled,
      std::function<void()> onRejected) override {
    (void)onFulfilled;
    (void)onRejected;
#if !defined(NDEBUG)
    std::cerr << "NoopSuspenseyCommitThenable received an unexpected listener." << std::endl;
#endif
  }
};

NoopSuspenseyCommitThenable& instance() {
  static NoopSuspenseyCommitThenable singleton;
  return singleton;
}

} // namespace

Wakeable& noopSuspenseyCommitThenable() {
  return instance();
}

bool isNoopSuspenseyCommitThenable(const Wakeable* wakeable) {
  if (wakeable == nullptr) {
    return false;
  }
  return wakeable == &instance();
}

bool isNoopSuspenseyCommitThenable(const void* value) {
  return isNoopSuspenseyCommitThenable(tryGetWakeable(value));
}

} // namespace react
