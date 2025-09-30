#include "react-reconciler/ReactFiberLane.h"
#include "shared/ReactFeatureFlags.h"

#include <cassert>

namespace react::test {

bool runReactFiberLaneRuntimeTests() {
  FiberRoot root;
  root.pendingLanes = SyncLane;
  root.suspendedLanes = NoLanes;
  root.pingedLanes = NoLanes;

  assert(getHighestPriorityPendingLanePriority(root) == LanePriority::SyncLanePriority);
  assert(getHighestPriorityPendingLane(root) == SyncLane);
  auto highestSet = getHighestPriorityPendingLanes(root);
  assert(highestSet == SyncLane);

  markRootUpdated(root, TransitionLane2);
  assert((root.pendingLanes & TransitionLane2) == TransitionLane2);
  if (enableDefaultTransitionIndicator) {
    assert((root.indicatorLanes & TransitionLane2) == TransitionLane2);
  }
  assert(root.suspendedLanes == NoLanes);
  assert(root.pingedLanes == NoLanes);
  assert(root.warmLanes == NoLanes);

  const int startTime = 1000;
  markStarvedLanesAsExpired(root, startTime);
  const auto syncIndex = laneToIndex(SyncLane);
  assert(root.expirationTimes[syncIndex] == startTime + syncLaneExpirationMs);
  assert((root.expiredLanes & SyncLane) == NoLanes);

  const int laterTime = startTime + syncLaneExpirationMs + 1;
  markStarvedLanesAsExpired(root, laterTime);
  assert((root.expiredLanes & SyncLane) == SyncLane);

  root = FiberRoot{};
  root.pendingLanes = RetryLane1;
  markStarvedLanesAsExpired(root, startTime);
  const auto retryIndex = laneToIndex(RetryLane1);
  if (enableRetryLaneExpiration) {
    assert(root.expirationTimes[retryIndex] != NoTimestamp);
  } else {
    assert(root.expirationTimes[retryIndex] == NoTimestamp);
  }

  root.pendingLanes = SyncLane | TransitionLane2;
  assert(getHighestPriorityPendingLanePriority(root) == LanePriority::SyncLanePriority);
  assert(getHighestPriorityPendingLane(root) == SyncLane);

  {
    FiberRoot entangleRoot;
    entangleRoot.entangledLanes = InputContinuousLane;
    entangleRoot.entanglements[laneToIndex(InputContinuousLane)] = DefaultLane;
    const auto entangled = getEntangledLanes(entangleRoot, InputContinuousLane);
    assert((entangled & (InputContinuousLane | DefaultLane)) == (InputContinuousLane | DefaultLane));
  }

  {
    FiberRoot retryRoot;
    retryRoot.pendingLanes = DefaultLane | OffscreenLane;
    const auto lanes = getLanesToRetrySynchronouslyOnError(retryRoot, SyncLane);
    assert(lanes == DefaultLane);
    retryRoot.errorRecoveryDisabledLanes = DefaultLane;
    assert(getLanesToRetrySynchronouslyOnError(retryRoot, DefaultLane) == NoLanes);
  }

  {
    FiberRoot expiredRoot;
    expiredRoot.expiredLanes = SyncLane;
    assert(includesExpiredLane(expiredRoot, SyncLane));
    assert(!includesExpiredLane(expiredRoot, TransitionLane1));
  }

  {
    FiberRoot suspendedRoot;
    const auto transitionIndex = laneToIndex(TransitionLane1);
    suspendedRoot.expirationTimes[transitionIndex] = 1234;
    markRootSuspended(suspendedRoot, TransitionLane1, NoLane, true);
    assert((suspendedRoot.suspendedLanes & TransitionLane1) == TransitionLane1);
    assert((suspendedRoot.pingedLanes & TransitionLane1) == NoLanes);
    assert((suspendedRoot.warmLanes & TransitionLane1) == TransitionLane1);
    assert(suspendedRoot.expirationTimes[transitionIndex] == NoTimestamp);
  }

  {
    FiberRoot spawnedRoot;
    markRootSuspended(spawnedRoot, TransitionLane2, DeferredLane, false);
    assert((spawnedRoot.pendingLanes & DeferredLane) == DeferredLane);
    assert((spawnedRoot.suspendedLanes & DeferredLane) == NoLanes);
    const auto deferredIndex = laneToIndex(DeferredLane);
    const auto entangledLanes = spawnedRoot.entanglements[deferredIndex];
    assert((entangledLanes & DeferredLane) == DeferredLane);
    assert((entangledLanes & TransitionLane2) == TransitionLane2);
  }

  {
    FiberRoot pingRoot;
    pingRoot.suspendedLanes = TransitionLane3;
    pingRoot.warmLanes = TransitionLane3;
    markRootPinged(pingRoot, TransitionLane3);
    assert((pingRoot.pingedLanes & TransitionLane3) == TransitionLane3);
    assert((pingRoot.warmLanes & TransitionLane3) == NoLanes);
  }

  {
    FiberRoot entangledRoot;
    markRootEntangled(entangledRoot, DefaultLane | TransitionLane4);
    assert((entangledRoot.entangledLanes & (DefaultLane | TransitionLane4)) == (DefaultLane | TransitionLane4));
    const auto defaultIndex = laneToIndex(DefaultLane);
    assert((entangledRoot.entanglements[defaultIndex] & (DefaultLane | TransitionLane4)) == (DefaultLane | TransitionLane4));
    const auto transitionIndex = laneToIndex(TransitionLane4);
    assert((entangledRoot.entanglements[transitionIndex] & (DefaultLane | TransitionLane4)) == (DefaultLane | TransitionLane4));
  }

  {
    FiberRoot finishRoot;
    finishRoot.tag = RootTag::ConcurrentRoot;
    finishRoot.pendingLanes = SyncLane | RetryLane1;
    finishRoot.suspendedLanes = RetryLane1;
    finishRoot.pingedLanes = RetryLane1;
    finishRoot.warmLanes = RetryLane1;
    finishRoot.expiredLanes = RetryLane1;
    finishRoot.entangledLanes = RetryLane1;
    finishRoot.errorRecoveryDisabledLanes = RetryLane1;
    finishRoot.shellSuspendCounter = 3;
    const auto retryIndex = laneToIndex(RetryLane1);
    finishRoot.entanglements[retryIndex] = RetryLane1;
    finishRoot.expirationTimes[retryIndex] = 42;
    ConcurrentUpdate hidden{};
    hidden.lane = RetryLane1 | OffscreenLane;
    finishRoot.hiddenUpdates[retryIndex] = std::vector<ConcurrentUpdate*>{&hidden};

    markRootFinished(finishRoot, RetryLane1, SyncLane, NoLane, NoLanes, RetryLane1);

    assert(finishRoot.pendingLanes == SyncLane);
    assert(finishRoot.suspendedLanes == RetryLane1);
    assert(finishRoot.pingedLanes == NoLanes);
    assert(finishRoot.warmLanes == NoLanes);
    assert(finishRoot.shellSuspendCounter == 0);
    assert(finishRoot.entanglements[retryIndex] == NoLanes);
    assert(finishRoot.expirationTimes[retryIndex] == NoTimestamp);
    assert(!finishRoot.hiddenUpdates[retryIndex].has_value());
    assert(hidden.lane == RetryLane1);
  }

  {
    FiberRoot upgradeRoot;
    upgradeRoot.entanglements[SyncLaneIndex] = NoLanes;
    upgradePendingLanesToSync(upgradeRoot, DefaultLane | TransitionLane1);
    assert((upgradeRoot.pendingLanes & SyncLane) == SyncLane);
    assert((upgradeRoot.entangledLanes & SyncLane) == SyncLane);
    const bool includesDefault = (upgradeRoot.entanglements[SyncLaneIndex] & DefaultLane) == DefaultLane;
    const bool includesTransition = (upgradeRoot.entanglements[SyncLaneIndex] & TransitionLane1) == TransitionLane1;
    assert(includesDefault && includesTransition);
  }

  {
    FiberRoot hiddenRoot;
    ConcurrentUpdate update{};
    markHiddenUpdate(hiddenRoot, &update, DefaultLane);
    const auto index = laneToIndex(DefaultLane);
    assert(hiddenRoot.hiddenUpdates[index].has_value());
    assert(hiddenRoot.hiddenUpdates[index]->size() == 1);
    assert(hiddenRoot.hiddenUpdates[index]->at(0) == &update);
    assert(update.lane == (DefaultLane | OffscreenLane));
  }

  {
    FiberRoot hydrationRoot;
    Lane bumped = getBumpedLaneForHydration(hydrationRoot, SyncLane);
    assert(bumped == SyncHydrationLane);
    hydrationRoot.suspendedLanes = SyncHydrationLane;
    bumped = getBumpedLaneForHydration(hydrationRoot, SyncLane);
    assert(bumped == NoLane);
    assert(getBumpedLaneForHydrationByLane(DefaultLane) == DefaultHydrationLane);
  }

  {
    assert(getGroupNameOfHighestPriorityLane(SyncLane) == std::string_view{"Blocking"});
    assert(getGroupNameOfHighestPriorityLane(TransitionLane1) == std::string_view{"Transition"});
    assert(getGroupNameOfHighestPriorityLane(RetryLane1) == std::string_view{"Suspense"});
    assert(getGroupNameOfHighestPriorityLane(IdleLane) == std::string_view{"Idle"});
    assert(getGroupNameOfHighestPriorityLane(OffscreenLane) == std::string_view{"Idle"});
    assert(getGroupNameOfHighestPriorityLane(NoLanes) == std::string_view{"Other"});
  }

  {
    FiberRoot transitionRoot;
    Transition dummyTransition;
    addTransitionToLanesMap(transitionRoot, &dummyTransition, TransitionLane2);
    const auto transitions = getTransitionsForLanes(transitionRoot, TransitionLane2);
    assert(transitions.empty());
    clearTransitionsForLanes(transitionRoot, TransitionLane2);
    assert(!transitionRoot.transitionLanes[laneToIndex(TransitionLane2)].has_value());
  }

  return true;
}

} // namespace react::test
