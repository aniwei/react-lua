#include "ReactRuntime.h"
#include "../react-dom/client/ReactDOMComponent.h"
#include <chrono>
#include <memory>
#include <utility>

namespace jsi = facebook::jsi;

namespace react {

ReactRuntime::ReactRuntime()
  : ReactFiberWorkLoop(*this, *this),
    schedulerEpoch_(std::chrono::steady_clock::now()) {}

std::shared_ptr<ReactDOMInstance> ReactRuntime::createInstance(
  jsi::Runtime& rt,
  const std::string& type,
  const jsi::Object& props) {
  return std::make_shared<ReactDOMComponent>(rt, type, props);
}

std::shared_ptr<ReactDOMInstance> ReactRuntime::createTextInstance(
  jsi::Runtime& rt,
  const std::string& text) {
  return std::make_shared<ReactDOMComponent>(rt, "#text", text);
}

void ReactRuntime::appendChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  if (!parent || !child) {
    return;
  }
  parent->appendChild(child);
}

void ReactRuntime::removeChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child) {
  if (!parent || !child) {
    return;
  }
  parent->removeChild(child);
}

void ReactRuntime::insertBefore(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild) {
  if (!parent || !child) {
    return;
  }
  parent->insertChildBefore(child, beforeChild);
}

void ReactRuntime::commitUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const jsi::Object& oldProps,
    const jsi::Object& newProps,
    const jsi::Object& updatePayload) {
  if (!instance) {
    return;
  }

  if (auto domInstance = std::dynamic_pointer_cast<ReactDOMComponent>(instance)) {
    domInstance->applyUpdate(newProps, updatePayload);
  }
}

TaskHandle ReactRuntime::scheduleTask(
  SchedulerPriority priority,
  Task task,
  const TaskOptions& options) {
  if (!task) {
    return {};
  }

  const uint64_t id = nextTaskId_++;
  TaskHandle handle{id};
  ScheduledTaskRecord record;
  record.task = std::move(task);
  record.priority = priority;
  record.options = options;

  pendingTasks_.emplace(id, std::move(record));
  runPendingTask(handle);
  return handle;
}

void ReactRuntime::cancelTask(TaskHandle handle) {
  if (!handle) {
    return;
  }

  auto it = pendingTasks_.find(handle.id);
  if (it == pendingTasks_.end()) {
    return;
  }

  it->second.cancelled = true;
  pendingTasks_.erase(it);
}

SchedulerPriority ReactRuntime::getCurrentPriorityLevel() const {
  return currentPriorityLevel_;
}

SchedulerPriority ReactRuntime::runWithPriority(
  SchedulerPriority priority,
  const std::function<void()>& fn) {
  SchedulerPriority previous = currentPriorityLevel_;
  currentPriorityLevel_ = priority;
  if (fn) {
    fn();
  }
  currentPriorityLevel_ = previous;
  return previous;
}

bool ReactRuntime::shouldYield() const {
  return false;
}

double ReactRuntime::now() const {
  auto elapsed = std::chrono::steady_clock::now() - schedulerEpoch_;
  auto ms = std::chrono::duration<double, std::milli>(elapsed);
  return ms.count();
}

void ReactRuntime::runPendingTask(TaskHandle handle) {
  if (!handle) {
    return;
  }

  auto it = pendingTasks_.find(handle.id);
  if (it == pendingTasks_.end()) {
    return;
  }

  ScheduledTaskRecord record = std::move(it->second);
  pendingTasks_.erase(it);
  if (record.cancelled || !record.task) {
    return;
  }

  SchedulerPriority previous = currentPriorityLevel_;
  currentPriorityLevel_ = record.priority;
  record.task();
  currentPriorityLevel_ = previous;
}

} // namespace react
