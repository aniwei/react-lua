#pragma once

#include "react-reconciler/ReactFiberLane.h"

#include <functional>
#include <memory>

namespace react {

class ReactRuntime;
struct Transition;

struct AsyncActionThenable {
  enum class Status {
    Pending,
    Fulfilled,
    Rejected,
  };

  Status status{Status::Pending};
};

using AsyncActionThenablePtr = std::shared_ptr<AsyncActionThenable>;

[[nodiscard]] Lane peekEntangledActionLane(ReactRuntime& runtime);
[[nodiscard]] AsyncActionThenablePtr peekEntangledActionThenable(ReactRuntime& runtime);

AsyncActionThenablePtr entangleAsyncAction(
  ReactRuntime& runtime,
  const Transition* transition,
  AsyncActionThenablePtr thenable);

void setEntangledActionForTesting(
  ReactRuntime& runtime,
  Lane lane,
  AsyncActionThenablePtr thenable);
void clearEntangledActionForTesting(ReactRuntime& runtime);

void registerDefaultIndicator(
  ReactRuntime& runtime,
  FiberRoot* root,
  std::function<std::function<void()>()> onDefaultTransitionIndicator);
void startIsomorphicDefaultIndicatorIfNeeded(ReactRuntime& runtime);
[[nodiscard]] bool hasOngoingIsomorphicIndicator(ReactRuntime& runtime);
[[nodiscard]] std::function<void()> retainIsomorphicIndicator(ReactRuntime& runtime);
void markIsomorphicIndicatorHandled(ReactRuntime& runtime);

} // namespace react
