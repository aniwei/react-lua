#include "ReactFiberConcurrentUpdates.h"

#include "ReactFiberLane.h"
#include "../reconciler/FiberNode.h"

#include <array>
#include <vector>

namespace react {

struct FiberRoot;

FiberRoot* markUpdateLaneFromFiberToRoot(FiberNode* sourceFiber, ConcurrentUpdate* update, Lane lane);
FiberRoot* getRootForUpdatedFiber(FiberNode* sourceFiber);

namespace {

struct ConcurrentQueueEntry {
	FiberNode* fiber{nullptr};
	ConcurrentUpdateQueue* queue{nullptr};
	ConcurrentUpdate* update{nullptr};
	Lane lane{NoLane};
};

std::vector<ConcurrentQueueEntry> gConcurrentQueueEntries;
Lanes gConcurrentlyUpdatedLanesLocal = NoLanes;

void enqueueUpdate(
	FiberNode* fiber,
	ConcurrentUpdateQueue* queue,
	ConcurrentUpdate* update,
	Lane lane) {
	gConcurrentQueueEntries.push_back(ConcurrentQueueEntry{fiber, queue, update, lane});
	gConcurrentlyUpdatedLanesLocal = mergeLanes(gConcurrentlyUpdatedLanesLocal, lane);

	if (fiber != nullptr) {
		fiber->lanes = mergeLanes(fiber->lanes, lane);
		if (fiber->alternate) {
			fiber->alternate->lanes = mergeLanes(fiber->alternate->lanes, lane);
		}
	}
}

} // namespace

void finishQueueingConcurrentUpdates() {
	const auto entryCount = gConcurrentQueueEntries.size();

	for (std::size_t i = 0; i < entryCount; ++i) {
		const auto& entry = gConcurrentQueueEntries[i];

		if (entry.queue != nullptr && entry.update != nullptr) {
			ConcurrentUpdate* pending = entry.queue->pending;
			if (pending == nullptr) {
				entry.update->next = entry.update;
			} else {
				entry.update->next = pending->next;
				pending->next = entry.update;
			}
			entry.queue->pending = entry.update;
		}

		if (entry.lane != NoLane && entry.fiber != nullptr) {
			markUpdateLaneFromFiberToRoot(entry.fiber, entry.update, entry.lane);
		}
	}

	gConcurrentQueueEntries.clear();
	gConcurrentlyUpdatedLanesLocal = NoLanes;
}

Lanes getConcurrentlyUpdatedLanes() {
	return gConcurrentlyUpdatedLanesLocal;
}

FiberRoot* enqueueConcurrentHookUpdate(
	FiberNode* fiber,
	ConcurrentUpdateQueue* queue,
	ConcurrentUpdate* update,
	Lane lane) {
	enqueueUpdate(fiber, queue, update, lane);
	return getRootForUpdatedFiber(fiber);
}

void enqueueConcurrentHookUpdateAndEagerlyBailout(
	FiberNode* fiber,
	ConcurrentUpdateQueue* queue,
	ConcurrentUpdate* update) {
	enqueueUpdate(fiber, queue, update, NoLane);
	// TODO: Match React's conditional flush once getWorkInProgressRoot wiring is available.
	finishQueueingConcurrentUpdates();
}

FiberRoot* enqueueConcurrentClassUpdate(
	FiberNode* fiber,
	ConcurrentUpdateQueue* queue,
	ConcurrentUpdate* update,
	Lane lane) {
	enqueueUpdate(fiber, queue, update, lane);
	return getRootForUpdatedFiber(fiber);
}

FiberRoot* enqueueConcurrentRenderForLane(FiberNode* fiber, Lane lane) {
	enqueueUpdate(fiber, nullptr, nullptr, lane);
	return getRootForUpdatedFiber(fiber);
}

FiberRoot* unsafe_markUpdateLaneFromFiberToRoot(FiberNode* fiber, Lane lane) {
	return markUpdateLaneFromFiberToRoot(fiber, nullptr, lane);
}

FiberRoot* markUpdateLaneFromFiberToRoot(
	FiberNode* sourceFiber,
	ConcurrentUpdate* update,
	Lane lane) {
	if (sourceFiber == nullptr) {
		return nullptr;
	}

	sourceFiber->lanes = mergeLanes(sourceFiber->lanes, lane);
	if (sourceFiber->alternate) {
		sourceFiber->alternate->lanes = mergeLanes(sourceFiber->alternate->lanes, lane);
	}

	bool isHidden = false;
	FiberNode* node = sourceFiber;
	FiberNode* parent = node->returnFiber.get();
	while (parent != nullptr) {
		parent->childLanes = mergeLanes(parent->childLanes, lane);
		if (parent->alternate) {
			parent->alternate->childLanes = mergeLanes(parent->alternate->childLanes, lane);
		}

		if (parent->tag == WorkTag::OffscreenComponent) {
			auto* offscreenInstance = static_cast<OffscreenInstance*>(parent->stateNode);
			if (offscreenInstance != nullptr && (offscreenInstance->_visibility & OffscreenVisible) == 0) {
				isHidden = true;
			}
		}

		node = parent;
		parent = parent->returnFiber.get();
	}

	if (node->tag == WorkTag::HostRoot) {
		auto* root = static_cast<FiberRoot*>(node->stateNode);
		if (root != nullptr) {
			markRootUpdated(*root, lane);
			if (isHidden && update != nullptr) {
				markHiddenUpdate(*root, update, lane);
			}
		}
		return root;
	}

	return nullptr;
}

FiberRoot* getRootForUpdatedFiber(FiberNode* sourceFiber) {
	if (sourceFiber == nullptr) {
		return nullptr;
	}

	FiberNode* node = sourceFiber;
	FiberNode* parent = node->returnFiber.get();
	while (parent != nullptr) {
		node = parent;
		parent = parent->returnFiber.get();
	}

	if (node->tag == WorkTag::HostRoot) {
		return static_cast<FiberRoot*>(node->stateNode);
	}
	return nullptr;
}

} // namespace react
