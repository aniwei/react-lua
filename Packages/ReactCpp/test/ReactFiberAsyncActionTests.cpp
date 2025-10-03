#include "react-reconciler/ReactFiberAsyncAction.h"
#include "react-reconciler/ReactFiberRootScheduler.h"
#include "runtime/ReactRuntime.h"
#include "shared/ReactFeatureFlags.h"

#include <cassert>
#include <memory>

namespace react::test {

#if REACTCPP_ENABLE_EXPERIMENTAL

namespace {

std::function<void()> sharedDefaultIndicator() {
  return []() {};
}

std::function<void()> conflictingDefaultIndicator() {
  return []() {};
}

} // namespace

bool runReactFiberAsyncActionTests() {
  ReactRuntime runtime;
  auto& state = runtime.asyncActionState();

  FiberRoot rootA{};
  FiberRoot rootB{};

  auto cleanupCounter = std::make_shared<int>(0);

  registerRootDefaultIndicator(runtime, rootA, sharedDefaultIndicator);
  assert(state.indicatorRegistrationState == IsomorphicIndicatorRegistrationState::Registered);
  assert(state.indicatorRegistrationRoot == &rootA);

  state.pendingIsomorphicIndicator = [cleanupCounter]() { ++(*cleanupCounter); };
  state.pendingEntangledRoots = 1;

  registerRootDefaultIndicator(runtime, rootB, sharedDefaultIndicator);
  assert(state.indicatorRegistrationState == IsomorphicIndicatorRegistrationState::Registered);
  assert(state.indicatorRegistrationRoot == &rootA);

  registerRootDefaultIndicator(runtime, rootB, conflictingDefaultIndicator);
  assert(state.indicatorRegistrationState == IsomorphicIndicatorRegistrationState::Disabled);
  assert(state.indicatorRegistrationRoot == nullptr);
  assert(state.pendingEntangledRoots == 0);
  assert(!state.pendingIsomorphicIndicator);
  assert(*cleanupCounter == 1);

  return true;
}

#else

bool runReactFiberAsyncActionTests() {
  return true;
}

#endif

} // namespace react::test
