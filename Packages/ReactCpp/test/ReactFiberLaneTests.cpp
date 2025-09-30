#include "react-reconciler/ReactFiberLane.h"

namespace react {
namespace {

static_assert(TotalLanes == 31, "TotalLanes mismatch");
static_assert(NoLane == 0u, "NoLane mismatch");
static_assert(NoLanes == 0u, "NoLanes mismatch");
static_assert(SyncHydrationLane == 0b1u, "SyncHydrationLane mask mismatch");
static_assert(SyncLane == (1u << 1), "SyncLane mask mismatch");
static_assert(InputContinuousLane == (1u << 3), "InputContinuousLane mask mismatch");
static_assert(DefaultLane == (1u << 5), "DefaultLane mask mismatch");
static_assert(GestureLane == (1u << 6), "GestureLane mask mismatch");
static_assert(TransitionLane14 == (1u << 21), "TransitionLane14 mask mismatch");
static_assert(RetryLane4 == (1u << 25), "RetryLane4 mask mismatch");
static_assert(SelectiveHydrationLane == (1u << 26), "SelectiveHydrationLane mask mismatch");
static_assert(IdleLane == (1u << 28), "IdleLane mask mismatch");
static_assert(DeferredLane == (1u << 30), "DeferredLane mask mismatch");

static_assert(SyncUpdateLanes == (SyncLane | InputContinuousLane | DefaultLane), "SyncUpdateLanes composition mismatch");
static_assert(UpdateLanes == (SyncUpdateLanes | TransitionLanes), "UpdateLanes composition mismatch");
static_assert(HydrationLanes == (
    SyncHydrationLane |
    InputContinuousHydrationLane |
    DefaultHydrationLane |
    TransitionHydrationLane |
    SelectiveHydrationLane |
    IdleHydrationLane), "HydrationLanes composition mismatch");

constexpr auto kLaneMap = createLaneMap<int>(-1);
static_assert(kLaneMap.size() == TotalLanes, "Lane map size mismatch");
static_assert(kLaneMap[0] == -1, "Lane map initial fill mismatch (index 0)");
static_assert(kLaneMap[TotalLanes - 1] == -1, "Lane map initial fill mismatch (last index)");

static_assert(includesSomeLane(SyncLane | DefaultLane, SyncLane), "includesSomeLane positive case mismatch");
static_assert(!includesSomeLane(SyncLane, DefaultLane), "includesSomeLane negative case mismatch");
static_assert(mergeLanes(SyncLane, DefaultLane) == (SyncLane | DefaultLane), "mergeLanes mismatch");
static_assert(subtractLanes(SyncLane | DefaultLane, DefaultLane) == SyncLane, "subtractLanes mismatch");
static_assert(removeLanes(SyncLane | DefaultLane, DefaultLane) == SyncLane, "removeLanes mismatch");
static_assert(intersectLanes(SyncLane | DefaultLane, DefaultLane) == DefaultLane, "intersectLanes mismatch");
static_assert(laneToLanes(SyncLane) == SyncLane, "laneToLanes mismatch");
static_assert(isSubsetOfLanes(SyncLane | DefaultLane, SyncLane), "isSubsetOfLanes single lane mismatch");
static_assert(!isSubsetOfLanes(SyncLane, SyncLane | DefaultLane), "isSubsetOfLanes superset mismatch");
static_assert(getHighestPriorityLane(TransitionLane3 | SyncLane) == SyncLane, "getHighestPriorityLane mismatch");
static_assert(higherPriorityLane(TransitionLane3, SyncLane) == SyncLane, "higherPriorityLane mismatch");
static_assert(includesSyncLane(SyncLane | DefaultLane), "includesSyncLane positive mismatch");
static_assert(!includesSyncLane(DefaultLane), "includesSyncLane negative mismatch");
static_assert(includesNonIdleWork(DefaultLane), "includesNonIdleWork mismatch");
static_assert(includesOnlyRetries(RetryLane1 | RetryLane2), "includesOnlyRetries mismatch");
static_assert(!includesOnlyRetries(RetryLane1 | SyncLane), "includesOnlyRetries negative mismatch");
static_assert(includesOnlyNonUrgentLanes(TransitionLane3 | IdleLane), "includesOnlyNonUrgentLanes mismatch");
static_assert(!includesOnlyNonUrgentLanes(SyncLane | TransitionLane1), "includesOnlyNonUrgentLanes negative mismatch");
static_assert(includesOnlyTransitions(TransitionLane4 | TransitionLane5), "includesOnlyTransitions mismatch");
static_assert(!includesOnlyTransitions(TransitionLane1 | RetryLane1), "includesOnlyTransitions negative mismatch");
static_assert(includesTransitionLane(TransitionLane1), "includesTransitionLane mismatch");
static_assert(!includesTransitionLane(RetryLane1), "includesTransitionLane negative mismatch");
static_assert(includesOnlyHydrationLanes(SyncHydrationLane | DefaultHydrationLane), "includesOnlyHydrationLanes mismatch");
static_assert(!includesOnlyHydrationLanes(SyncHydrationLane | DefaultLane), "includesOnlyHydrationLanes negative mismatch");
static_assert(includesOnlyOffscreenLanes(OffscreenLane), "includesOnlyOffscreenLanes mismatch");
static_assert(!includesOnlyOffscreenLanes(OffscreenLane | IdleLane), "includesOnlyOffscreenLanes negative mismatch");
static_assert(includesOnlyHydrationOrOffscreenLanes(SyncHydrationLane | OffscreenLane), "includesOnlyHydrationOrOffscreenLanes mismatch");
static_assert(!includesOnlyHydrationOrOffscreenLanes(SyncHydrationLane | DefaultLane), "includesOnlyHydrationOrOffscreenLanes negative mismatch");
static_assert(includesOnlyViewTransitionEligibleLanes(TransitionLane1 | RetryLane1 | IdleLane), "includesOnlyViewTransitionEligibleLanes mismatch");
static_assert(!includesOnlyViewTransitionEligibleLanes(TransitionLane1 | GestureLane), "includesOnlyViewTransitionEligibleLanes negative mismatch");
static_assert(includesOnlySuspenseyCommitEligibleLanes(TransitionLane2 | RetryLane1 | IdleLane | GestureLane), "includesOnlySuspenseyCommitEligibleLanes mismatch");
static_assert(!includesOnlySuspenseyCommitEligibleLanes(TransitionLane2 | SyncLane), "includesOnlySuspenseyCommitEligibleLanes negative mismatch");
static_assert(includesLoadingIndicatorLanes(SyncLane | RetryLane1), "includesLoadingIndicatorLanes mismatch");
static_assert(!includesLoadingIndicatorLanes(TransitionLane1), "includesLoadingIndicatorLanes negative mismatch");
static_assert(includesBlockingLane(DefaultLane | TransitionLane1), "includesBlockingLane mismatch");
static_assert(!includesBlockingLane(TransitionLane1 | RetryLane1), "includesBlockingLane negative mismatch");
static_assert(isBlockingLane(DefaultLane), "isBlockingLane mismatch");
static_assert(!isBlockingLane(TransitionLane1), "isBlockingLane negative mismatch");
static_assert(isTransitionLane(TransitionLane1), "isTransitionLane mismatch");
static_assert(!isTransitionLane(RetryLane1), "isTransitionLane negative mismatch");
static_assert(isGestureRender(GestureLane), "isGestureRender mismatch");
static_assert(!isGestureRender(SyncLane), "isGestureRender negative mismatch");
static_assert(getHighestPriorityLanes(SyncLane | DefaultLane) == (SyncLane | DefaultLane), "getHighestPriorityLanes sync mismatch");
static_assert(getHighestPriorityLanes(TransitionLane3 | TransitionLane4) == (TransitionLane3 | TransitionLane4), "getHighestPriorityLanes transition mismatch");
static_assert(lanePriorityForLane(SyncLane) == LanePriority::SyncLanePriority, "lanePriorityForLane Sync mismatch");
static_assert(lanePriorityForLane(DefaultLane) == LanePriority::DefaultLanePriority, "lanePriorityForLane Default mismatch");
static_assert(lanePriorityForLane(TransitionLane3) == LanePriority::TransitionLanePriority, "lanePriorityForLane Transition mismatch");
static_assert(lanePriorityForLane(IdleLane) == LanePriority::IdleLanePriority, "lanePriorityForLane Idle mismatch");
static_assert(lanesToLanePriority(SyncLane | DefaultLane) == LanePriority::SyncLanePriority, "lanesToLanePriority precedence mismatch");
static_assert(isLanePriorityHigher(LanePriority::SyncLanePriority, LanePriority::DefaultLanePriority), "isLanePriorityHigher mismatch");
static_assert(higherLanePriority(LanePriority::RetryLanePriority, LanePriority::TransitionLanePriority) == LanePriority::TransitionLanePriority, "higherLanePriority mismatch");

} // namespace
} // namespace react
