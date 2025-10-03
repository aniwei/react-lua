#include "react-reconciler/ReactUpdateQueue.h"

#include "react-reconciler/ReactFiberAsyncAction.h"
#include "runtime/ReactRuntime.h"

#include <stdexcept>
#include <utility>

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
  update->tag = UpdateTag::UpdateState;
  update->payload = std::move(payload);
  update->next = nullptr;
  return update;
}

void enqueueUpdate(UpdateQueue& queue, const std::shared_ptr<Update>& update) {
  queue.ownedUpdates.push_back(update);

  auto* updatePtr = update.get();
  if (queue.shared.pending == nullptr) {
    updatePtr->next = updatePtr;
  } else {
    auto* pending = queue.shared.pending;
    updatePtr->next = pending->next;
    pending->next = updatePtr;
  }
  queue.shared.pending = updatePtr;
  queue.shared.lanes = mergeLanes(queue.shared.lanes, updatePtr->lane);
}

void appendPendingUpdates(UpdateQueue& queue) {
  const auto pending = queue.shared.pending;
  if (pending == nullptr) {
    return;
  }

  queue.shared.pending = nullptr;
  queue.shared.lanes = NoLanes;

  auto* lastPendingUpdate = pending;
  auto* firstPendingUpdate = static_cast<Update*>(pending->next);
  if (firstPendingUpdate == nullptr) {
    firstPendingUpdate = lastPendingUpdate;
  }
  lastPendingUpdate->next = nullptr;

  if (queue.firstBaseUpdate == nullptr) {
    queue.firstBaseUpdate = firstPendingUpdate;
  } else if (queue.lastBaseUpdate != nullptr) {
    queue.lastBaseUpdate->next = firstPendingUpdate;
  }

  queue.lastBaseUpdate = lastPendingUpdate;
}

namespace {

bool gDidReadFromEntangledAsyncAction = false;
bool gHasForceUpdate = false;

void callCallback(const std::function<void()>& callback) {
  if (!callback) {
    throw std::invalid_argument(
        "Invalid argument passed as callback. Expected a function.");
  }
  callback();
}

void applyUpdateReducer(Update* update, jsi::Value& state) {
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

const jsi::Value& processUpdateQueue(ReactRuntime& runtime, UpdateQueue& queue) {
  appendPendingUpdates(queue);

  jsi::Value newState = std::move(queue.baseState);
  gDidReadFromEntangledAsyncAction = false;
  gHasForceUpdate = false;
  queue.callbacks.clear();

  auto* current = queue.firstBaseUpdate;
  while (current != nullptr) {
  if (current->lane != NoLane && current->lane == peekEntangledActionLane(runtime)) {
      gDidReadFromEntangledAsyncAction = true;
    }

    switch (current->tag) {
      case UpdateTag::UpdateState:
      case UpdateTag::CaptureUpdate:
      case UpdateTag::ReplaceState:
        applyUpdateReducer(current, newState);
        break;
      case UpdateTag::ForceUpdate:
        // ForceUpdate intentionally does not modify state in this simplified port.
        gHasForceUpdate = true;
        break;
    }

    if (current->callback) {
      queue.callbacks.push_back(current->callback);
    }

    current = static_cast<Update*>(current->next);
  }

  queue.baseState = std::move(newState);
  queue.firstBaseUpdate = nullptr;
  queue.lastBaseUpdate = nullptr;
  queue.ownedUpdates.clear();

  return queue.baseState;
}

void suspendIfUpdateReadFromEntangledAsyncAction(ReactRuntime& runtime) {
  if (!gDidReadFromEntangledAsyncAction) {
    return;
  }

  if (auto thenable = peekEntangledActionThenable(runtime)) {
    throw thenable;
  }
}

void resetHasForceUpdateBeforeProcessing() {
  gHasForceUpdate = false;
}

bool checkHasForceUpdateAfterProcessing() {
  return gHasForceUpdate;
}

void deferHiddenCallbacks(UpdateQueue& queue) {
  if (queue.callbacks.empty()) {
    return;
  }

  auto& hiddenCallbacks = queue.shared.hiddenCallbacks;
  hiddenCallbacks.reserve(hiddenCallbacks.size() + queue.callbacks.size());
  for (auto& callback : queue.callbacks) {
    hiddenCallbacks.push_back(std::move(callback));
  }
  queue.callbacks.clear();
}

void commitHiddenCallbacks(UpdateQueue& queue) {
  auto hiddenCallbacks = std::move(queue.shared.hiddenCallbacks);
  queue.shared.hiddenCallbacks.clear();
  for (auto& callback : hiddenCallbacks) {
    callCallback(callback);
  }
}

void commitCallbacks(UpdateQueue& queue) {
  auto callbacks = std::move(queue.callbacks);
  queue.callbacks.clear();
  for (auto& callback : callbacks) {
    callCallback(callback);
  }
}

} // namespace react
