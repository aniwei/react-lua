#include "shared/ReactFeatureFlags.h"
#include "shared/ReactFiberFlags.h"
#include "shared/ReactSymbols.h"
#include "shared/ReactWorkTags.h"

#include <string_view>

namespace react {
namespace {

constexpr bool matches(
    const ReactSymbolDescriptor& descriptor,
    std::string_view identifier,
    ReactSymbolDescriptor::Kind kind) {
  return descriptor.kind == kind && identifier == descriptor.identifier;
}

// ReactWorkTags snapshots ----------------------------------------------------

static_assert(toInt(WorkTag::FunctionComponent) == 0, "FunctionComponent tag mismatch");
static_assert(toInt(WorkTag::ClassComponent) == 1, "ClassComponent tag mismatch");
static_assert(toInt(WorkTag::HostRoot) == 3, "HostRoot tag mismatch");
static_assert(toInt(WorkTag::HostComponent) == 5, "HostComponent tag mismatch");
static_assert(toInt(WorkTag::HostText) == 6, "HostText tag mismatch");
static_assert(toInt(WorkTag::SuspenseComponent) == 13, "SuspenseComponent tag mismatch");
static_assert(toInt(WorkTag::OffscreenComponent) == 22, "OffscreenComponent tag mismatch");
static_assert(toInt(WorkTag::TracingMarkerComponent) == 25, "TracingMarkerComponent tag mismatch");
static_assert(toInt(WorkTag::ViewTransitionComponent) == 30, "ViewTransitionComponent tag mismatch");
static_assert(toInt(WorkTag::ActivityComponent) == 31, "ActivityComponent tag mismatch");

// ReactFiberFlags snapshots --------------------------------------------------

static_assert(NoFlags == 0u, "NoFlags mismatch");
static_assert(PerformedWork == (1u << 0), "PerformedWork flag mismatch");
static_assert(Placement == (1u << 1), "Placement flag mismatch");
static_assert(Update == (1u << 2), "Update flag mismatch");
static_assert(ChildDeletion == (1u << 4), "ChildDeletion flag mismatch");
static_assert(Passive == (1u << 11), "Passive flag mismatch");
static_assert(Visibility == (1u << 13), "Visibility flag mismatch");
static_assert(StoreConsistency == (1u << 14), "StoreConsistency flag mismatch");
static_assert(Incomplete == (1u << 15), "Incomplete flag mismatch");
static_assert(ShouldCapture == (1u << 16), "ShouldCapture flag mismatch");
static_assert(Forked == (1u << 20), "Forked flag mismatch");
static_assert(ViewTransitionStatic == (1u << 25), "ViewTransitionStatic flag mismatch");
static_assert(PlacementDEV == (1u << 26), "PlacementDEV flag mismatch");
static_assert(MountLayoutDev == (1u << 27), "MountLayoutDev flag mismatch");
static_assert(MountPassiveDev == (1u << 28), "MountPassiveDev flag mismatch");
static_assert(
    HostEffectMask == 0b0000000000000000111111111111111u,
    "HostEffectMask snapshot mismatch");
static_assert(
    StaticMask ==
        (LayoutStatic | PassiveStatic | RefStatic | MaySuspendCommit |
         ViewTransitionStatic | ViewTransitionNamedStatic),
    "StaticMask composition mismatch");

static_assert(
    BeforeMutationMask == computeBeforeMutationMask(FeatureFlags{}),
    "BeforeMutationMask default computation mismatch");
static_assert(
    computeBeforeMutationMask(FeatureFlags{true, false}) ==
        (Snapshot | Update | ChildDeletion | Visibility),
    "BeforeMutationMask createEventHandle case mismatch");
static_assert(
    computeBeforeMutationMask(FeatureFlags{false, true}) ==
        (Snapshot | Update),
    "BeforeMutationMask useEffectEvent case mismatch");

// ReactFeatureFlags snapshots ------------------------------------------------

static_assert(enableHydrationLaneScheduling, "enableHydrationLaneScheduling mismatch");
static_assert(!disableSchedulerTimeoutInWorkLoop, "disableSchedulerTimeoutInWorkLoop mismatch");
static_assert(!enableSuspenseCallback, "enableSuspenseCallback mismatch");
static_assert(!enableScopeAPI, "enableScopeAPI mismatch");
static_assert(!enableCreateEventHandleAPI, "enableCreateEventHandleAPI mismatch");
static_assert(!enableLegacyFBSupport, "enableLegacyFBSupport mismatch");
static_assert(!enableYieldingBeforePassive, "enableYieldingBeforePassive mismatch");
static_assert(!enableThrottledScheduling, "enableThrottledScheduling mismatch");
static_assert(enableLegacyCache == kExperimentalBuild, "enableLegacyCache mismatch");
static_assert(enableAsyncIterableChildren == kExperimentalBuild, "enableAsyncIterableChildren mismatch");
static_assert(enableTaint == kExperimentalBuild, "enableTaint mismatch");
static_assert(enablePostpone == kExperimentalBuild, "enablePostpone mismatch");
static_assert(enableHalt == kExperimentalBuild, "enableHalt mismatch");
static_assert(enableViewTransition == kExperimentalBuild, "enableViewTransition mismatch");
static_assert(enableGestureTransition == kExperimentalBuild, "enableGestureTransition mismatch");
static_assert(enableScrollEndPolyfill == kExperimentalBuild, "enableScrollEndPolyfill mismatch");
static_assert(!enableSuspenseyImages, "enableSuspenseyImages mismatch");
static_assert(enableFizzBlockingRender == kExperimentalBuild, "enableFizzBlockingRender mismatch");
static_assert(enableSrcObject == kExperimentalBuild, "enableSrcObject mismatch");
static_assert(enableHydrationChangeEvent == kExperimentalBuild, "enableHydrationChangeEvent mismatch");
static_assert(enableDefaultTransitionIndicator == kExperimentalBuild, "enableDefaultTransitionIndicator mismatch");
static_assert(!enableObjectFiber, "enableObjectFiber mismatch");
static_assert(!enableTransitionTracing, "enableTransitionTracing mismatch");
static_assert(!enableLegacyHidden, "enableLegacyHidden mismatch");
static_assert(!enableSuspenseAvoidThisFallback, "enableSuspenseAvoidThisFallback mismatch");
static_assert(enableCPUSuspense == kExperimentalBuild, "enableCPUSuspense mismatch");
static_assert(!enableNoCloningMemoCache, "enableNoCloningMemoCache mismatch");
static_assert(enableUseEffectEventHook == kExperimentalBuild, "enableUseEffectEventHook mismatch");
static_assert(enableFizzExternalRuntime == kExperimentalBuild, "enableFizzExternalRuntime mismatch");
static_assert(alwaysThrottleRetries, "alwaysThrottleRetries mismatch");
static_assert(!passChildrenWhenCloningPersistedNodes, "passChildrenWhenCloningPersistedNodes mismatch");
static_assert(!enablePersistedModeClonedFlag, "enablePersistedModeClonedFlag mismatch");
static_assert(enableEagerAlternateStateNodeCleanup, "enableEagerAlternateStateNodeCleanup mismatch");
static_assert(!enableRetryLaneExpiration, "enableRetryLaneExpiration mismatch");
static_assert(retryLaneExpirationMs == 5000, "retryLaneExpirationMs mismatch");
static_assert(syncLaneExpirationMs == 250, "syncLaneExpirationMs mismatch");
static_assert(transitionLaneExpirationMs == 5000, "transitionLaneExpirationMs mismatch");
static_assert(!enableInfiniteRenderLoopDetection, "enableInfiniteRenderLoopDetection mismatch");
static_assert(enableFragmentRefs == kExperimentalBuild, "enableFragmentRefs mismatch");
static_assert(renameElementSymbol, "renameElementSymbol mismatch");
static_assert(!enableHiddenSubtreeInsertionEffectCleanup, "enableHiddenSubtreeInsertionEffectCleanup mismatch");
static_assert(disableLegacyContext, "disableLegacyContext mismatch");
static_assert(disableLegacyContextForFunctionComponents, "disableLegacyContextForFunctionComponents mismatch");
static_assert(!enableMoveBefore, "enableMoveBefore mismatch");
static_assert(disableClientCache, "disableClientCache mismatch");
static_assert(enableReactTestRendererWarning, "enableReactTestRendererWarning mismatch");
static_assert(disableLegacyMode, "disableLegacyMode mismatch");
static_assert(disableCommentsAsDOMContainers, "disableCommentsAsDOMContainers mismatch");
static_assert(!enableTrustedTypesIntegration, "enableTrustedTypesIntegration mismatch");
static_assert(!disableInputAttributeSyncing, "disableInputAttributeSyncing mismatch");
static_assert(!disableTextareaChildren, "disableTextareaChildren mismatch");
static_assert(enableProfilerTimer == kProfileBuild, "enableProfilerTimer mismatch");
static_assert(enableComponentPerformanceTrack == kExperimentalBuild, "enableComponentPerformanceTrack mismatch");
static_assert(
    enableSchedulingProfiler == (!enableComponentPerformanceTrack && kProfileBuild),
    "enableSchedulingProfiler mismatch");
static_assert(enableProfilerCommitHooks == kProfileBuild, "enableProfilerCommitHooks mismatch");
static_assert(enableProfilerNestedUpdatePhase == kProfileBuild, "enableProfilerNestedUpdatePhase mismatch");
static_assert(enableAsyncDebugInfo, "enableAsyncDebugInfo mismatch");
static_assert(enableUpdaterTracking == kProfileBuild, "enableUpdaterTracking mismatch");
static_assert(ownerStackLimit == 10000, "ownerStackLimit mismatch");

// ReactSymbols snapshots -----------------------------------------------------

static_assert(
    matches(REACT_LEGACY_ELEMENT_TYPE, "react.element", ReactSymbolDescriptor::Kind::Registry),
    "REACT_LEGACY_ELEMENT_TYPE mismatch");
static_assert(
    matches(
        REACT_TRANSITIONAL_ELEMENT_TYPE,
        "react.transitional.element",
        ReactSymbolDescriptor::Kind::Registry),
    "REACT_TRANSITIONAL_ELEMENT_TYPE mismatch");
static_assert(
    matches(REACT_ELEMENT_TYPE,
            renameElementSymbol ? "react.transitional.element" : "react.element",
            ReactSymbolDescriptor::Kind::Registry),
    "REACT_ELEMENT_TYPE mismatch");
static_assert(
    matches(REACT_PORTAL_TYPE, "react.portal", ReactSymbolDescriptor::Kind::Registry),
    "REACT_PORTAL_TYPE mismatch");
static_assert(
    matches(REACT_SUSPENSE_TYPE, "react.suspense", ReactSymbolDescriptor::Kind::Registry),
    "REACT_SUSPENSE_TYPE mismatch");
static_assert(
    matches(REACT_SUSPENSE_LIST_TYPE, "react.suspense_list", ReactSymbolDescriptor::Kind::Registry),
    "REACT_SUSPENSE_LIST_TYPE mismatch");
static_assert(
    matches(REACT_MEMO_TYPE, "react.memo", ReactSymbolDescriptor::Kind::Registry),
    "REACT_MEMO_TYPE mismatch");
static_assert(
    matches(REACT_LAZY_TYPE, "react.lazy", ReactSymbolDescriptor::Kind::Registry),
    "REACT_LAZY_TYPE mismatch");
static_assert(
    matches(REACT_SCOPE_TYPE, "react.scope", ReactSymbolDescriptor::Kind::Registry),
    "REACT_SCOPE_TYPE mismatch");
static_assert(
    matches(REACT_ACTIVITY_TYPE, "react.activity", ReactSymbolDescriptor::Kind::Registry),
    "REACT_ACTIVITY_TYPE mismatch");
static_assert(
    matches(REACT_LEGACY_HIDDEN_TYPE, "react.legacy_hidden", ReactSymbolDescriptor::Kind::Registry),
    "REACT_LEGACY_HIDDEN_TYPE mismatch");
static_assert(
    matches(REACT_TRACING_MARKER_TYPE, "react.tracing_marker", ReactSymbolDescriptor::Kind::Registry),
    "REACT_TRACING_MARKER_TYPE mismatch");
static_assert(
    matches(REACT_MEMO_CACHE_SENTINEL, "react.memo_cache_sentinel", ReactSymbolDescriptor::Kind::Registry),
    "REACT_MEMO_CACHE_SENTINEL mismatch");
static_assert(
    matches(REACT_POSTPONE_TYPE, "react.postpone", ReactSymbolDescriptor::Kind::Registry),
    "REACT_POSTPONE_TYPE mismatch");
static_assert(
    matches(REACT_VIEW_TRANSITION_TYPE, "react.view_transition", ReactSymbolDescriptor::Kind::Registry),
    "REACT_VIEW_TRANSITION_TYPE mismatch");
static_assert(
    matches(ASYNC_ITERATOR, "asyncIterator", ReactSymbolDescriptor::Kind::Builtin),
    "ASYNC_ITERATOR mismatch");

static_assert(
    (renameElementSymbol ? REACT_TRANSITIONAL_ELEMENT_TYPE : REACT_LEGACY_ELEMENT_TYPE) ==
        REACT_ELEMENT_TYPE,
    "REACT_ELEMENT_TYPE conditional mismatch");

} // namespace
} // namespace react
