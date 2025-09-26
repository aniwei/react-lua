#pragma once

#include "jsi/jsi.h"
#include <functional>
#include <memory>
#include <utility>

namespace jsi = facebook::jsi;

namespace react {

using Lane = uint32_t;
using Lanes = uint32_t;

struct Update;

struct SharedQueue {
  std::shared_ptr<Update> pending;
};

struct Update {
  Lanes lane{0};
  jsi::Value payload;
  std::function<jsi::Value(jsi::Runtime&, const jsi::Value&)> reducer;
  std::function<void(jsi::Runtime&)> callback;
  std::shared_ptr<Update> next;
};

struct UpdateQueue {
  jsi::Value baseState;
  std::shared_ptr<Update> firstBaseUpdate;
  std::shared_ptr<Update> lastBaseUpdate;
  SharedQueue shared;
  std::function<void(jsi::Runtime&)> effects;

  UpdateQueue();
};

std::shared_ptr<UpdateQueue> createUpdateQueue(jsi::Runtime& rt, const jsi::Value& baseState);
std::shared_ptr<Update> createUpdate(Lanes lane, jsi::Value payload);
void enqueueUpdate(UpdateQueue& queue, const std::shared_ptr<Update>& update);

} // namespace react
