#pragma once

#include "react-reconciler/ReactFiberLane.h"

namespace react {

class ReactRuntime;

void ensureRootIsScheduled(ReactRuntime& runtime, FiberRoot& root);

} // namespace react
