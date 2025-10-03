#include "react-reconciler/ReactFiberAsyncAction.h"

#include "react-reconciler/ReactFiberRootScheduler.h"
#include "runtime/ReactRuntime.h"
#include "shared/ReactFeatureFlags.h"

#include <exception>
#include <iostream>
#include <memory>
#include <typeinfo>
#include <utility>

namespace react {
namespace {

AsyncActionState& getAsyncActionState(ReactRuntime& runtime) {
  return runtime.asyncActionState();
}

const AsyncActionState& getAsyncActionState(const ReactRuntime& runtime) {
  return runtime.asyncActionState();
}

void reportIndicatorError(const std::exception& ex) {
  std::cerr << "React default transition indicator threw: " << ex.what() << std::endl;
}

void reportIndicatorUnknownError() {
  std::cerr << "React default transition indicator threw an unknown exception" << std::endl;
}

using DefaultIndicatorFunctionPtr = std::function<void()> (*)();

struct IndicatorIdentity {
  const std::type_info* type{nullptr};
  const void* token{nullptr};
};

IndicatorIdentity deriveIndicatorIdentity(const std::function<std::function<void()>()>& callback) {
  IndicatorIdentity identity{};
  if (!callback) {
    return identity;
  }

  identity.type = &callback.target_type();

  if (const auto* fnPtr = callback.target<DefaultIndicatorFunctionPtr>()) {
    identity.token = reinterpret_cast<const void*>(*fnPtr);
  }

  return identity;
}

bool canShareIndicator(const AsyncActionState& state, const IndicatorIdentity& incomingIdentity, const FiberRoot* incomingRoot) {
  if (state.indicatorRegistrationRoot == incomingRoot) {
    return true;
  }

  if (state.indicatorRegistrationToken != nullptr && incomingIdentity.token != nullptr) {
    return state.indicatorRegistrationToken == incomingIdentity.token;
  }

  return false;
}

void stopIsomorphicDefaultIndicator(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  auto& state = getAsyncActionState(runtime);
  if (state.pendingIsomorphicIndicator) {
    auto cleanup = state.pendingIsomorphicIndicator;
    state.pendingIsomorphicIndicator = {};
    try {
      cleanup();
    } catch (const std::exception& ex) {
      reportIndicatorError(ex);
    } catch (...) {
      reportIndicatorUnknownError();
    }
  }
}

void disableIsomorphicDefaultIndicator(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  auto& state = getAsyncActionState(runtime);
  stopIsomorphicDefaultIndicator(runtime);
  state.indicatorRegistrationState = IsomorphicIndicatorRegistrationState::Disabled;
  state.isomorphicDefaultTransitionIndicator = {};
  state.indicatorRegistrationRoot = nullptr;
  state.indicatorRegistrationType = nullptr;
  state.indicatorRegistrationToken = nullptr;
  state.pendingEntangledRoots = 0;
  state.needsIsomorphicIndicator = false;
}

} // namespace

AsyncActionThenablePtr entangleAsyncAction(
  ReactRuntime& runtime,
  const Transition* transition,
  AsyncActionThenablePtr thenable) {
  auto& state = getAsyncActionState(runtime);

  if (state.currentEntangledActionLane == NoLane) {
    state.currentEntangledActionLane = requestTransitionLane(runtime, transition);
  }

  if (!thenable) {
    thenable = std::make_shared<AsyncActionThenable>();
  }

  state.currentEntangledActionThenable = thenable;
  state.currentEntangledActionThenable->status = AsyncActionThenable::Status::Pending;

  if (enableDefaultTransitionIndicator) {
    state.needsIsomorphicIndicator = true;
    ensureScheduleIsScheduled(runtime);
  }

  return state.currentEntangledActionThenable;
}

Lane peekEntangledActionLane(ReactRuntime& runtime) {
  return getAsyncActionState(runtime).currentEntangledActionLane;
}

AsyncActionThenablePtr peekEntangledActionThenable(ReactRuntime& runtime) {
  return getAsyncActionState(runtime).currentEntangledActionThenable;
}

void setEntangledActionForTesting(
  ReactRuntime& runtime,
  Lane lane,
  AsyncActionThenablePtr thenable) {
  auto& state = getAsyncActionState(runtime);
  state.currentEntangledActionLane = lane;
  state.currentEntangledActionThenable = std::move(thenable);
}

void clearEntangledActionForTesting(ReactRuntime& runtime) {
  auto& state = getAsyncActionState(runtime);
  state.currentEntangledActionLane = NoLane;
  state.currentEntangledActionThenable.reset();
  state.needsIsomorphicIndicator = false;
  state.pendingEntangledRoots = 0;
  stopIsomorphicDefaultIndicator(runtime);
}

void registerDefaultIndicator(
  ReactRuntime& runtime,
  FiberRoot* root,
  std::function<std::function<void()>()> onDefaultTransitionIndicator) {
  if (!enableDefaultTransitionIndicator || root == nullptr) {
    return;
  }

  auto& state = getAsyncActionState(runtime);

  if (!onDefaultTransitionIndicator) {
    if (state.indicatorRegistrationRoot == root &&
        state.indicatorRegistrationState == IsomorphicIndicatorRegistrationState::Registered) {
      stopIsomorphicDefaultIndicator(runtime);
      state.indicatorRegistrationState = IsomorphicIndicatorRegistrationState::Uninitialized;
      state.isomorphicDefaultTransitionIndicator = {};
      state.indicatorRegistrationRoot = nullptr;
      state.indicatorRegistrationType = nullptr;
      state.indicatorRegistrationToken = nullptr;
      state.pendingEntangledRoots = 0;
      state.needsIsomorphicIndicator = false;
    }
    return;
  }

  const IndicatorIdentity identity = deriveIndicatorIdentity(onDefaultTransitionIndicator);

  switch (state.indicatorRegistrationState) {
    case IsomorphicIndicatorRegistrationState::Uninitialized: {
      state.indicatorRegistrationState = IsomorphicIndicatorRegistrationState::Registered;
      state.indicatorRegistrationRoot = root;
      state.indicatorRegistrationType = identity.type;
      state.indicatorRegistrationToken = identity.token;
      state.isomorphicDefaultTransitionIndicator = std::move(onDefaultTransitionIndicator);
      break;
    }
    case IsomorphicIndicatorRegistrationState::Registered: {
      if (state.indicatorRegistrationRoot == root) {
        state.isomorphicDefaultTransitionIndicator = std::move(onDefaultTransitionIndicator);
        state.indicatorRegistrationType = identity.type;
        state.indicatorRegistrationToken = identity.token;
        break;
      }

      if (canShareIndicator(state, identity, root)) {
        break;
      }

      disableIsomorphicDefaultIndicator(runtime);
      break;
    }
    case IsomorphicIndicatorRegistrationState::Disabled:
      break;
  }
}

void startIsomorphicDefaultIndicatorIfNeeded(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }

  auto& state = getAsyncActionState(runtime);
  if (!state.needsIsomorphicIndicator) {
    return;
  }
  if (state.indicatorRegistrationState != IsomorphicIndicatorRegistrationState::Registered) {
    return;
  }
  if (state.pendingIsomorphicIndicator) {
    return;
  }

  try {
    if (state.isomorphicDefaultTransitionIndicator) {
      auto cleanup = state.isomorphicDefaultTransitionIndicator();
      if (cleanup) {
        state.pendingIsomorphicIndicator = std::move(cleanup);
      } else {
        state.pendingIsomorphicIndicator = []() {};
      }
    } else {
      state.pendingIsomorphicIndicator = []() {};
    }
  } catch (const std::exception& ex) {
    state.pendingIsomorphicIndicator = []() {};
    reportIndicatorError(ex);
  } catch (...) {
    state.pendingIsomorphicIndicator = []() {};
    reportIndicatorUnknownError();
  }
}

bool hasOngoingIsomorphicIndicator(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return false;
  }
  return static_cast<bool>(getAsyncActionState(runtime).pendingIsomorphicIndicator);
}

std::function<void()> retainIsomorphicIndicator(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return []() {};
  }

  auto& state = getAsyncActionState(runtime);
  ++state.pendingEntangledRoots;
  auto releaseFlag = std::make_shared<bool>(false);
  return [runtimePtr = &runtime, releaseFlag]() mutable {
    if (*releaseFlag) {
      return;
    }
    *releaseFlag = true;
    auto& capturedState = runtimePtr->asyncActionState();
    if (capturedState.pendingEntangledRoots > 0) {
      --capturedState.pendingEntangledRoots;
      if (capturedState.pendingEntangledRoots == 0) {
        stopIsomorphicDefaultIndicator(*runtimePtr);
      }
    }
  };
}

void markIsomorphicIndicatorHandled(ReactRuntime& runtime) {
  if (!enableDefaultTransitionIndicator) {
    return;
  }
  getAsyncActionState(runtime).needsIsomorphicIndicator = false;
}

} // namespace react
