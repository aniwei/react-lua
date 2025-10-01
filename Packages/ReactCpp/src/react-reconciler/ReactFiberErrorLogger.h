#pragma once

#include "react-reconciler/ReactCapturedValue.h"
#include "react-reconciler/ReactFiberLane.h"

namespace react {

void logUncaughtError(FiberRoot& root, const CapturedValue& errorInfo);

} // namespace react
