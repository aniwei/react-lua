#pragma once

#include "react-reconciler/ReactFiberAsyncAction.h"
#include "react-reconciler/ReactFiberRootSchedulerState.h"
#include "react-reconciler/ReactFiberWorkLoopState.h"
#include "scheduler/Scheduler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <typeinfo>
#include <unordered_map>

namespace facebook {
namespace jsi {
class Runtime;
class Object;
} // namespace jsi
} // namespace facebook

namespace react {

class HostInterface;
class ReactDOMInstance;
struct FiberRoot;

enum class IsomorphicIndicatorRegistrationState : std::uint8_t {
  Uninitialized = 0,
  Registered = 1,
  Disabled = 2,
};

struct AsyncActionState {
  Lane currentEntangledActionLane{NoLane};
  AsyncActionThenablePtr currentEntangledActionThenable{};
  IsomorphicIndicatorRegistrationState indicatorRegistrationState{
    IsomorphicIndicatorRegistrationState::Uninitialized};
  std::function<std::function<void()>()> isomorphicDefaultTransitionIndicator{};
  std::function<void()> pendingIsomorphicIndicator{};
  std::size_t pendingEntangledRoots{0};
  bool needsIsomorphicIndicator{false};
  FiberRoot* indicatorRegistrationRoot{nullptr};
  const std::type_info* indicatorRegistrationType{nullptr};
  const void* indicatorRegistrationToken{nullptr};
};

class ReactRuntime {
public:
  ReactRuntime();

  WorkLoopState& workLoopState();
  const WorkLoopState& workLoopState() const;
  RootSchedulerState& rootSchedulerState();
  const RootSchedulerState& rootSchedulerState() const;
  AsyncActionState& asyncActionState();
  const AsyncActionState& asyncActionState() const;

  void resetWorkLoop();
  void resetRootScheduler();

  void setHostInterface(std::shared_ptr<HostInterface> hostInterface);
  void bindHostInterface(facebook::jsi::Runtime& runtime);
  void reset();

  void setShouldAttemptEagerTransitionCallback(std::function<bool()> callback);
  [[nodiscard]] bool shouldAttemptEagerTransition() const;
  void renderRootSync(
    facebook::jsi::Runtime& runtime,
    std::uint32_t rootElementOffset,
    std::shared_ptr<ReactDOMInstance> rootContainer);
  void hydrateRoot(
    facebook::jsi::Runtime& runtime,
    std::uint32_t rootElementOffset,
    std::shared_ptr<ReactDOMInstance> rootContainer);

  void unregisterRootContainer(const ReactDOMInstance* rootContainer);

  [[nodiscard]] std::size_t getRegisteredRootCount() const;

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

  std::shared_ptr<ReactDOMInstance> createInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& type,
    const facebook::jsi::Object& props);

  std::shared_ptr<ReactDOMInstance> createTextInstance(
    facebook::jsi::Runtime& runtime,
    const std::string& text);

  void appendChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child);

  void removeChild(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child);

  void insertBefore(
    std::shared_ptr<ReactDOMInstance> parent,
    std::shared_ptr<ReactDOMInstance> child,
    std::shared_ptr<ReactDOMInstance> beforeChild);

  void commitUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const facebook::jsi::Object& oldProps,
    const facebook::jsi::Object& newProps,
    const facebook::jsi::Object& payload);

  void commitTextUpdate(
    std::shared_ptr<ReactDOMInstance> instance,
    const std::string& oldText,
    const std::string& newText);

private:
  std::shared_ptr<HostInterface> ensureHostInterface();
  void registerRootContainer(const std::shared_ptr<ReactDOMInstance>& rootContainer);

  std::shared_ptr<HostInterface> hostInterface_{};
  WorkLoopState workLoopState_{};
  RootSchedulerState rootSchedulerState_{};
  AsyncActionState asyncActionState_{};
  SchedulerPriority currentPriority_{SchedulerPriority::NormalPriority};
  std::uint64_t nextTaskId_{1};
  std::function<bool()> shouldAttemptEagerTransitionCallback_{};
  std::unordered_map<const ReactDOMInstance*, std::weak_ptr<ReactDOMInstance>> registeredRoots_{};
};

namespace ReactRuntimeTestHelper {
std::size_t getRegisteredRootCount(const ReactRuntime& runtime);
}

} // namespace react
