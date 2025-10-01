#include "react-reconciler/ReactUpdateQueue.h"
#include "react-reconciler/ReactFiberAsyncAction.h"

#include <cassert>
#include <memory>

namespace react::test {

namespace {

bool testBasicQueueProcessing() {
  UpdateQueue queue;
  queue.baseState = jsi::Value(0);

  const auto update1 = createUpdate(DefaultLane);
  update1->reducer = [](const jsi::Value& prev) {
    return jsi::Value(prev.getNumber() + 1);
  };
  enqueueUpdate(queue, update1);
  assert(queue.shared.pending == update1.get());
  assert(queue.shared.pending->next == update1.get());
  assert(queue.shared.lanes == update1->lane);

  const auto update2 = createUpdate(DefaultLane);
  update2->reducer = [](const jsi::Value& prev) {
    return jsi::Value(prev.getNumber() + 2);
  };
  enqueueUpdate(queue, update2);
  const auto pending = queue.shared.pending;
  assert(pending == update2.get());
  assert(pending->next == update1.get());
  assert(queue.shared.lanes == DefaultLane);

  appendPendingUpdates(queue);
  assert(queue.shared.pending == nullptr);
  assert(queue.shared.lanes == NoLanes);
  assert(queue.firstBaseUpdate == update1.get());
  assert(queue.lastBaseUpdate == update2.get());

  const auto& finalState = processUpdateQueue(queue);
  assert(finalState.isNumber());
  assert(finalState.getNumber() == 3);
  assert(queue.baseState.isNumber());
  assert(queue.baseState.getNumber() == 3);
  assert(queue.firstBaseUpdate == nullptr);
  assert(queue.lastBaseUpdate == nullptr);
  assert(queue.callbacks.empty());

  return true;
}

bool testForceUpdateTracking() {
  UpdateQueue queue;
  queue.baseState = jsi::Value(1);

  const auto update = createUpdate(DefaultLane);
  update->tag = UpdateTag::ForceUpdate;
  enqueueUpdate(queue, update);

  resetHasForceUpdateBeforeProcessing();
  processUpdateQueue(queue);
  assert(checkHasForceUpdateAfterProcessing());

  return true;
}

bool testDeferredHiddenCallbacks() {
  bool callbackInvoked = false;

  UpdateQueue queue;
  queue.baseState = jsi::Value(0);

  const auto update = createUpdate(DefaultLane);
  update->reducer = [](const jsi::Value& prev) {
    return jsi::Value(prev.getNumber() + 1);
  };
  update->callback = [&]() { callbackInvoked = true; };
  enqueueUpdate(queue, update);

  processUpdateQueue(queue);
  assert(!queue.callbacks.empty());
  deferHiddenCallbacks(queue);
  assert(queue.callbacks.empty());
  assert(queue.shared.hiddenCallbacks.size() == 1);
  assert(!callbackInvoked);

  commitHiddenCallbacks(queue);
  assert(queue.shared.hiddenCallbacks.empty());
  assert(callbackInvoked);

  return true;
}

bool testCommitCallbacks() {
  bool callbackInvoked = false;

  UpdateQueue queue;
  queue.baseState = jsi::Value(0);

  const auto update = createUpdate(DefaultLane);
  update->reducer = [](const jsi::Value& prev) {
    return jsi::Value(prev.getNumber() + 1);
  };
  update->callback = [&]() { callbackInvoked = true; };
  enqueueUpdate(queue, update);

  processUpdateQueue(queue);
  commitCallbacks(queue);
  assert(callbackInvoked);
  assert(queue.callbacks.empty());

  return true;
}

bool testEntangledAsyncActionSuspension() {
  clearEntangledActionForTesting();

  UpdateQueue queue;
  queue.baseState = jsi::Value(0);

  const auto update = createUpdate(DefaultLane);
  update->reducer = [](const jsi::Value& prev) {
    return jsi::Value(prev.getNumber() + 1);
  };
  enqueueUpdate(queue, update);

  auto thenable = std::make_shared<AsyncActionThenable>();
  setEntangledActionForTesting(DefaultLane, thenable);

  processUpdateQueue(queue);

  bool threw = false;
  try {
    suspendIfUpdateReadFromEntangledAsyncAction();
  } catch (const AsyncActionThenablePtr& thrown) {
    assert(thrown == thenable);
    threw = true;
  }
  assert(threw);

  clearEntangledActionForTesting();

  return true;
}

} // namespace

bool runUpdateQueueTests() {
  return testBasicQueueProcessing() && testForceUpdateTracking() &&
      testDeferredHiddenCallbacks() && testCommitCallbacks() &&
      testEntangledAsyncActionSuspension();
}

} // namespace react::test
