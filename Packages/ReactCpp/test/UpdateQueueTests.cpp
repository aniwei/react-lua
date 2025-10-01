#include "react-reconciler/ReactUpdateQueue.h"

#include <cassert>

namespace react::test {

bool runUpdateQueueTests() {
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

} // namespace react::test
