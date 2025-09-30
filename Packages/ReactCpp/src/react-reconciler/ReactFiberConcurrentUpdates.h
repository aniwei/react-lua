#pragma once

#include "ReactFiberFlags.h"
#include "ReactFiberLane.h"
#include "ReactFiberOffscreenComponent.h"

namespace react {

class FiberNode;

struct ConcurrentUpdateQueue {
	ConcurrentUpdate* pending{nullptr};
};

void finishQueueingConcurrentUpdates();
[[nodiscard]] Lanes getConcurrentlyUpdatedLanes();

FiberRoot* enqueueConcurrentHookUpdate(FiberNode* fiber, ConcurrentUpdateQueue* queue, ConcurrentUpdate* update, Lane lane);
void enqueueConcurrentHookUpdateAndEagerlyBailout(FiberNode* fiber, ConcurrentUpdateQueue* queue, ConcurrentUpdate* update);
FiberRoot* enqueueConcurrentClassUpdate(FiberNode* fiber, ConcurrentUpdateQueue* queue, ConcurrentUpdate* update, Lane lane);
FiberRoot* enqueueConcurrentRenderForLane(FiberNode* fiber, Lane lane);

FiberRoot* unsafe_markUpdateLaneFromFiberToRoot(FiberNode* fiber, Lane lane);

} // namespace react
