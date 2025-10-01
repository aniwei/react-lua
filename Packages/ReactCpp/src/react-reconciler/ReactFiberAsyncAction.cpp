#include "react-reconciler/ReactFiberAsyncAction.h"

#include <utility>

namespace react {

namespace {

Lane gCurrentEntangledActionLane = NoLane;
AsyncActionThenablePtr gCurrentEntangledActionThenable;

} // namespace

Lane peekEntangledActionLane() {
  return gCurrentEntangledActionLane;
}

AsyncActionThenablePtr peekEntangledActionThenable() {
  return gCurrentEntangledActionThenable;
}

void setEntangledActionForTesting(Lane lane, AsyncActionThenablePtr thenable) {
  gCurrentEntangledActionLane = lane;
  gCurrentEntangledActionThenable = std::move(thenable);
}

void clearEntangledActionForTesting() {
  gCurrentEntangledActionLane = NoLane;
  gCurrentEntangledActionThenable.reset();
}

} // namespace react
