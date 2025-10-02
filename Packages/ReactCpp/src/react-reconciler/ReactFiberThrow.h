#pragma once

#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactFiberWorkLoopState.h"

namespace react {

class FiberNode;
struct FiberRoot;
class ReactRuntime;

bool throwException(
    ReactRuntime& runtime,
    FiberRoot& root,
    FiberNode* returnFiber,
    FiberNode& unitOfWork,
    void* thrownValue,
    Lanes renderLanes);

} // namespace react
