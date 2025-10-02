#include "react-reconciler/ReactFiberHiddenContext.h"

#include "react-reconciler/ReactFiberWorkLoop.h"
#include "runtime/ReactRuntime.h"

namespace react {
namespace {

StackCursor<HiddenContextOptional> gCurrentTreeHiddenStackCursor =
    createCursor<HiddenContextOptional>(std::nullopt);
StackCursor<Lanes> gPrevEntangledRenderLanesCursor =
    createCursor<Lanes>(NoLanes);

} // namespace

StackCursor<HiddenContextOptional>& currentTreeHiddenStackCursor() {
  return gCurrentTreeHiddenStackCursor;
}

StackCursor<Lanes>& prevEntangledRenderLanesCursor() {
  return gPrevEntangledRenderLanesCursor;
}

void pushHiddenContext(ReactRuntime& runtime, FiberNode& fiber, const HiddenContext& context) {
  const auto prevEntangledRenderLanes = getEntangledRenderLanes(runtime);
  push(gPrevEntangledRenderLanesCursor, prevEntangledRenderLanes, &fiber);
  push(gCurrentTreeHiddenStackCursor, HiddenContextOptional{context}, &fiber);

  setEntangledRenderLanes(runtime, mergeLanes(prevEntangledRenderLanes, context.baseLanes));
}

void reuseHiddenContextOnStack(ReactRuntime& runtime, FiberNode& fiber) {
  const auto prevEntangledRenderLanes = getEntangledRenderLanes(runtime);
  push(gPrevEntangledRenderLanesCursor, prevEntangledRenderLanes, &fiber);
  push(gCurrentTreeHiddenStackCursor, gCurrentTreeHiddenStackCursor.current, &fiber);
}

void popHiddenContext(ReactRuntime& runtime, FiberNode& fiber) {
  setEntangledRenderLanes(runtime, gPrevEntangledRenderLanesCursor.current);
  pop(gCurrentTreeHiddenStackCursor, &fiber);
  pop(gPrevEntangledRenderLanesCursor, &fiber);
}

bool isCurrentTreeHidden() {
  return gCurrentTreeHiddenStackCursor.current.has_value();
}

} // namespace react
