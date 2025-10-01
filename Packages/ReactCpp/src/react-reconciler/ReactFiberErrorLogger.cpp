#include "react-reconciler/ReactFiberErrorLogger.h"

#include <iostream>

namespace react {

void logUncaughtError(FiberRoot& root, const CapturedValue& errorInfo) {
  (void)root;
  (void)errorInfo;
  // TODO: Integrate with runtime error callbacks and component stacks.
}

} // namespace react
