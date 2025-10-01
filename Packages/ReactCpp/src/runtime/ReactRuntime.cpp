#include "ReactRuntime.h"

#include <chrono>
#include <utility>

namespace react {

ReactRuntime::ReactRuntime() = default;

WorkLoopState& ReactRuntime::workLoopState() {
  return workLoopState_;
}

const WorkLoopState& ReactRuntime::workLoopState() const {
  return workLoopState_;
}

void ReactRuntime::resetWorkLoop() {
  workLoopState_ = WorkLoopState{};
}

void ReactRuntime::setHostInterface(std::shared_ptr<HostInterface> hostInterface) {
  hostInterface_ = std::move(hostInterface);
}

void ReactRuntime::bindHostInterface(facebook::jsi::Runtime& runtime) {
  (void)runtime;
  // TODO: hook host interface bindings once available.
}

void ReactRuntime::renderRootSync(
  facebook::jsi::Runtime& runtime,
  std::uint32_t rootElementOffset,
  std::shared_ptr<ReactDOMInstance> rootContainer) {
  (void)runtime;
  (void)rootElementOffset;
  (void)rootContainer;
  // TODO: implement render pipeline integration.
}

void ReactRuntime::hydrateRoot(
  facebook::jsi::Runtime& runtime,
  std::uint32_t rootElementOffset,
  std::shared_ptr<ReactDOMInstance> rootContainer) {
  (void)runtime;
  (void)rootElementOffset;
  (void)rootContainer;
  // TODO: implement hydration pipeline integration.
}

double ReactRuntime::now() const {
  const auto steadyNow = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(steadyNow).count();
}

} // namespace react
