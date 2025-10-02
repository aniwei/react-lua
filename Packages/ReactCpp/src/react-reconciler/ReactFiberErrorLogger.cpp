#include "react-reconciler/ReactFiberErrorLogger.h"

#include <iostream>

namespace react {

void logUncaughtError(FiberRoot& root, const CapturedValue& errorInfo) {
  (void)root;
  (void)errorInfo;
  // TODO: Integrate with runtime error callbacks and component stacks.
}

void logCaughtError(FiberRoot& root, FiberNode& fiber, const CapturedValue& errorInfo) {
  (void)root;
  (void)fiber;
  (void)errorInfo;
  // TODO: surface caught errors to devtools/runtime observers.
}

} // namespace react
