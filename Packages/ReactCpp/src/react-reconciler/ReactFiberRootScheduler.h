#pragma once

#include "react-reconciler/ReactFiberLane.h"

namespace react {

class ReactRuntime;

void ensureRootIsScheduled(ReactRuntime& runtime, FiberRoot& root);
void ensureScheduleIsScheduled(ReactRuntime& runtime);
void flushSyncWorkOnAllRoots(ReactRuntime& runtime, Lanes syncTransitionLanes);
void flushSyncWorkOnLegacyRootsOnly(ReactRuntime& runtime);
Lane requestTransitionLane(ReactRuntime& runtime, const Transition* transition);
bool didCurrentEventScheduleTransition(ReactRuntime& runtime);
void markIndicatorHandled(ReactRuntime& runtime, FiberRoot& root);
void registerRootDefaultIndicator(
	ReactRuntime& runtime,
	FiberRoot& root,
	std::function<std::function<void()>()> onDefaultTransitionIndicator);

} // namespace react
