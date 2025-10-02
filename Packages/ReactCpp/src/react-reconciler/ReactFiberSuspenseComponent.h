#pragma once

#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactWakeable.h"

#include <unordered_set>
#include <vector>

namespace react {

struct SuspenseProps {
  bool unstable_avoidThisFallback{false};
};

struct SuspenseState {
  void* dehydrated{nullptr};
  void* treeContext{nullptr};
  Lane retryLane{NoLane};
  std::vector<void*> hydrationErrors{};
};

using RetryQueue = std::unordered_set<const Wakeable*>;

} // namespace react
