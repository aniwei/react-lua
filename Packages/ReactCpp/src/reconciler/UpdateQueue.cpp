#include "UpdateQueue.h"

namespace jsi = facebook::jsi;

namespace react {

UpdateQueue::UpdateQueue()
  : baseState(jsi::Value::undefined()),
    firstBaseUpdate(nullptr),
    lastBaseUpdate(nullptr),
    shared{},
    callbacks() {}

std::shared_ptr<UpdateQueue> createUpdateQueue(jsi::Runtime& rt, const jsi::Value& baseState) {
  auto queue = std::make_shared<UpdateQueue>();
  queue->baseState = jsi::Value(rt, baseState);
  return queue;
}

std::shared_ptr<Update> createUpdate(Lane lane, jsi::Value payload) {
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
  queue.shared.lanes = mergeLanes(queue.shared.lanes, update->lane);
}

void appendPendingUpdates(UpdateQueue& queue) {
  const auto pending = queue.shared.pending;
  if (pending == nullptr) {
    return;
  }

  queue.shared.pending = nullptr;
  queue.shared.lanes = NoLanes;

  const auto lastPendingUpdate = pending;
  const auto firstPendingUpdate = pending->next;
  lastPendingUpdate->next = nullptr;

  if (queue.firstBaseUpdate == nullptr) {
    queue.firstBaseUpdate = firstPendingUpdate;
  } else if (queue.lastBaseUpdate != nullptr) {
    queue.lastBaseUpdate->next = firstPendingUpdate;
  }

  queue.lastBaseUpdate = lastPendingUpdate;
}

namespace {

void applyUpdateReducer(const std::shared_ptr<Update>& update, jsi::Value& state) {
  if (update->reducer) {
    jsi::Value result = update->reducer(state);
    state = std::move(result);
    return;
  }

  if (!update->payload.isUndefined()) {
    state = std::move(update->payload);
  }
}

} // namespace

const jsi::Value& processUpdateQueue(UpdateQueue& queue) {
  appendPendingUpdates(queue);

  jsi::Value newState = std::move(queue.baseState);
  queue.callbacks.clear();

  auto current = queue.firstBaseUpdate;
  while (current != nullptr) {
    switch (current->tag) {
      case UpdateTag::UpdateState:
      case UpdateTag::CaptureUpdate:
      case UpdateTag::ReplaceState:
        applyUpdateReducer(current, newState);
        break;
      case UpdateTag::ForceUpdate:
        // ForceUpdate intentionally does not modify state in this simplified port.
        break;
    }

    if (current->callback) {
      queue.callbacks.push_back(current->callback);
    }

    current = current->next;
  }

  queue.baseState = std::move(newState);
  queue.firstBaseUpdate = nullptr;
  queue.lastBaseUpdate = nullptr;

  return queue.baseState;
}

} // namespace react
