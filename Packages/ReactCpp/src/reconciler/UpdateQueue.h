#pragma once

#include "jsi/jsi.h"
#include "react-reconciler/ReactFiberLane.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

struct Update;

enum class UpdateTag : uint8_t {
  UpdateState = 0,
  ReplaceState = 1,
  ForceUpdate = 2,
  CaptureUpdate = 3,
};

struct SharedQueue {
  std::shared_ptr<Update> pending;
  Lanes lanes{NoLanes};
  std::vector<std::function<void()>> hiddenCallbacks;
};

struct Update {
  Lane lane{NoLane};
  UpdateTag tag{UpdateTag::UpdateState};
  jsi::Value payload;
  std::function<jsi::Value(const jsi::Value&)> reducer;
  std::function<void()> callback;
  std::shared_ptr<Update> next;
};

struct UpdateQueue {
  jsi::Value baseState;
  std::shared_ptr<Update> firstBaseUpdate;
  std::shared_ptr<Update> lastBaseUpdate;
  SharedQueue shared;
  std::vector<std::function<void()>> callbacks;

  UpdateQueue();
};

std::shared_ptr<UpdateQueue> createUpdateQueue(jsi::Runtime& rt, const jsi::Value& baseState);
std::shared_ptr<Update> createUpdate(Lane lane, jsi::Value payload = jsi::Value::undefined());
void enqueueUpdate(UpdateQueue& queue, const std::shared_ptr<Update>& update);
void appendPendingUpdates(UpdateQueue& queue);
const jsi::Value& processUpdateQueue(UpdateQueue& queue);

inline bool operator==(UpdateTag a, UpdateTag b) {
  return static_cast<uint8_t>(a) == static_cast<uint8_t>(b);
}

inline bool operator!=(UpdateTag a, UpdateTag b) {
  return !(a == b);
}

} // namespace react
