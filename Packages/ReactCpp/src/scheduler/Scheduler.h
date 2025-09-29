#pragma once

#include <cstdint>
#include <functional>

namespace react {

using Task = std::function<void()>;

enum class SchedulerPriority : uint8_t {
  NoPriority = 0,
  ImmediatePriority = 1,
  UserBlockingPriority = 2,
  NormalPriority = 3,
  LowPriority = 4,
  IdlePriority = 5,
};

struct TaskOptions {
  double delayMs{0.0};
  double timeoutMs{0.0};
};

struct TaskHandle {
  uint64_t id{0};

  explicit operator bool() const {
    return id != 0;
  }

  bool operator==(const TaskHandle& other) const {
    return id == other.id;
  }

  bool operator!=(const TaskHandle& other) const {
    return id != other.id;
  }
};

class Scheduler {
public:
  virtual ~Scheduler() = default;

  virtual TaskHandle scheduleTask(
    SchedulerPriority priority,
    Task task,
    const TaskOptions& options = {}) = 0;

  virtual void cancelTask(TaskHandle handle) = 0;

  virtual SchedulerPriority getCurrentPriorityLevel() const = 0;

  virtual SchedulerPriority runWithPriority(
    SchedulerPriority priority,
    const std::function<void()>& fn) = 0;

  virtual bool shouldYield() const = 0;

  virtual double now() const = 0;
};

} // namespace react
