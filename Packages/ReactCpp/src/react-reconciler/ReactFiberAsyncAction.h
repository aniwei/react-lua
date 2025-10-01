#pragma once

#include "react-reconciler/ReactFiberLane.h"

#include <memory>

namespace react {

struct AsyncActionThenable {
  enum class Status {
    Pending,
    Fulfilled,
    Rejected,
  };

  Status status{Status::Pending};
};

using AsyncActionThenablePtr = std::shared_ptr<AsyncActionThenable>;

[[nodiscard]] Lane peekEntangledActionLane();
[[nodiscard]] AsyncActionThenablePtr peekEntangledActionThenable();

void setEntangledActionForTesting(Lane lane, AsyncActionThenablePtr thenable);
void clearEntangledActionForTesting();

} // namespace react
