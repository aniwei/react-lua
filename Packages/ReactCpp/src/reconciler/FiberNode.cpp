#include "FiberNode.h"
#include <utility>

namespace jsi = facebook::jsi;

namespace react {

FiberNode::FiberNode(
  WorkTag tagValue, 
  jsi::Value pendingPropsValue, 
  jsi::Value keyValue, 
  uint32_t fiberMode,
  Lanes fiberLanes)
  : tag(tagValue),
    elementType(jsi::Value::undefined()),
    type(jsi::Value::undefined()),
    key(std::move(keyValue)),
    ref(jsi::Value::undefined()),
    stateNode(nullptr),
    returnFiber(nullptr),
    child(nullptr),
    sibling(nullptr),
    index(0),
    pendingProps(std::move(pendingPropsValue)),
    memoizedProps(jsi::Value::undefined()),
    pendingState(jsi::Value::undefined()),
    memoizedState(jsi::Value::undefined()),
    updateQueue(nullptr),
    dependencies(nullptr),
    mode(fiberMode),
    lanes(fiberLanes),
    childLanes(0),
    flags(FiberFlags::NoFlags),
    subtreeFlags(FiberFlags::NoFlags),
    deletions(),
    alternate(nullptr),
    actualDuration(0.0),
    actualStartTime(-1.0),
    selfBaseDuration(0.0),
    treeBaseDuration(0.0),
    isHydrating(false) {}

} // namespace react
