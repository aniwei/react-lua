#include "react-reconciler/ReactFiber.h"
#include "react-reconciler/ReactFiberConcurrentUpdates.h"
#include "react-reconciler/ReactFiberLane.h"
#include "react-reconciler/ReactFiberOffscreenComponent.h"
#include "react-reconciler/ReactWorkTags.h"
#include "shared/ReactFeatureFlags.h"

#include "jsi/jsi.h"

#include <cassert>
#include <memory>

namespace react::test {

namespace {

std::shared_ptr<FiberNode> makeFiber(WorkTag tag) {
  FiberNode* fiber = react::createFiber(tag);
  return std::shared_ptr<FiberNode>(fiber, [](FiberNode* ptr) { delete ptr; });
}

} // namespace

bool runReactFiberConcurrentUpdatesRuntimeTests() {
  FiberRoot rootState{};
  rootState.tag = RootTag::ConcurrentRoot;

  auto rootFiber = makeFiber(WorkTag::HostRoot);
  rootFiber->stateNode = &rootState;

  ConcurrentUpdateQueue queue{};
  ConcurrentUpdate update{};
  update.lane = DefaultLane;

  auto child = makeFiber(WorkTag::FunctionComponent);
  child->returnFiber = rootFiber.get();
  rootFiber->child = child.get();

  auto* scheduledRoot = enqueueConcurrentHookUpdate(child.get(), &queue, &update, DefaultLane);
  assert(scheduledRoot == &rootState);

  finishQueueingConcurrentUpdates();

  assert(queue.pending == &update);
  assert(update.next == &update);
  assert(rootState.pendingLanes == DefaultLane);
  assert(child->lanes == DefaultLane);
  assert(rootFiber->childLanes == DefaultLane);

  // Hidden update propagation
  ConcurrentUpdate hiddenUpdate{};
  hiddenUpdate.lane = TransitionLane1;

  OffscreenInstance hiddenInstance;
  hiddenInstance._visibility = 0;

  auto offscreen = makeFiber(WorkTag::OffscreenComponent);
  offscreen->stateNode = &hiddenInstance;
  offscreen->returnFiber = rootFiber.get();
  rootFiber->child = offscreen.get();

  auto hiddenChild = makeFiber(WorkTag::FunctionComponent);
  hiddenChild->returnFiber = offscreen.get();
  offscreen->child = hiddenChild.get();

  ConcurrentUpdateQueue hiddenQueue{};
  auto* hiddenRoot = enqueueConcurrentHookUpdate(hiddenChild.get(), &hiddenQueue, &hiddenUpdate, TransitionLane1);
  assert(hiddenRoot == &rootState);

  finishQueueingConcurrentUpdates();

  const auto index = laneToIndex(TransitionLane1);
  const auto& hiddenSlot = rootState.hiddenUpdates[index];
  if (hiddenSlot.has_value()) {
    bool containsUpdate = false;
    for (auto* entry : *hiddenSlot) {
      if (entry == &hiddenUpdate) {
        containsUpdate = true;
        break;
      }
    }
    assert(containsUpdate);
  } else {
    assert(false && "Expected hidden updates to be tracked");
  }
  assert((hiddenUpdate.lane & OffscreenLane) == OffscreenLane);

  return true;
}

} // namespace react::test
