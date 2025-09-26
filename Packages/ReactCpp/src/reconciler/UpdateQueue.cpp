#include "UpdateQueue.h"

namespace jsi = facebook::jsi;

namespace react {

UpdateQueue::UpdateQueue()
  : baseState(jsi::Value::undefined()),
    firstBaseUpdate(nullptr),
    lastBaseUpdate(nullptr),
    shared{nullptr},
    effects(nullptr) {}

std::shared_ptr<UpdateQueue> createUpdateQueue(jsi::Runtime& rt, const jsi::Value& baseState) {
  auto queue = std::make_shared<UpdateQueue>();
  queue->baseState = jsi::Value(rt, baseState);
  return queue;
}

std::shared_ptr<Update> createUpdate(Lanes lane, jsi::Value payload) {
  auto update = std::make_shared<Update>();
  update->lane = lane;
  update->payload = std::move(payload);
  update->next = nullptr;
  return update;
}

void enqueueUpdate(UpdateQueue& queue, const std::shared_ptr<Update>& update) {
  if (queue.shared.pending == nullptr) {
    update->next = update;
  } else {
    auto pending = queue.shared.pending;
    update->next = pending->next;
    pending->next = update;
  }
  queue.shared.pending = update;
}

} // namespace react
