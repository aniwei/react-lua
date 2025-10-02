#pragma once

// Simplified port of react-main/packages/react-reconciler/src/ReactFiberOffscreenComponent.js
// Provides minimal Offscreen visibility primitives required by the concurrent
// updates helpers.

#include "react-reconciler/ReactFiberSuspenseComponent.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace react {

using OffscreenVisibility = std::uint8_t;

inline constexpr OffscreenVisibility OffscreenVisible = 0b001;
inline constexpr OffscreenVisibility OffscreenPassiveEffectsConnected = 0b010;

struct OffscreenInstance {
	OffscreenVisibility _visibility{OffscreenVisible};
};

struct OffscreenQueue {
	std::vector<const Transition*> transitions{};
	std::vector<void*> markerInstances{};
	std::unique_ptr<RetryQueue> retryQueue{};
};

} // namespace react
