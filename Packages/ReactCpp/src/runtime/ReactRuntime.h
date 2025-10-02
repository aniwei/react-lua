#pragma once

#include "react-reconciler/ReactFiberWorkLoopState.h"
#include "scheduler/Scheduler.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace facebook {
namespace jsi {
class Runtime;
} // namespace jsi
} // namespace facebook

namespace react {

class HostInterface;
class ReactDOMInstance;

class ReactRuntime {
public:
  ReactRuntime();

  WorkLoopState& workLoopState();
  const WorkLoopState& workLoopState() const;

  void resetWorkLoop();

  void setHostInterface(std::shared_ptr<HostInterface> hostInterface);
  void bindHostInterface(facebook::jsi::Runtime& runtime);
  void renderRootSync(
    facebook::jsi::Runtime& runtime,
    std::uint32_t rootElementOffset,
    std::shared_ptr<ReactDOMInstance> rootContainer);
  void hydrateRoot(
    facebook::jsi::Runtime& runtime,
    std::uint32_t rootElementOffset,
    std::shared_ptr<ReactDOMInstance> rootContainer);

  TaskHandle scheduleTask(
    SchedulerPriority priority,
    Task task,
    const TaskOptions& options = {});

  void cancelTask(TaskHandle handle);

  SchedulerPriority getCurrentPriorityLevel() const;

  SchedulerPriority runWithPriority(
    SchedulerPriority priority,
    const std::function<void()>& fn);

  bool shouldYield() const;

  [[nodiscard]] double now() const;

private:
  std::shared_ptr<HostInterface> hostInterface_{};
  WorkLoopState workLoopState_{};
  SchedulerPriority currentPriority_{SchedulerPriority::NormalPriority};
  std::uint64_t nextTaskId_{1};
};

} // namespace react
