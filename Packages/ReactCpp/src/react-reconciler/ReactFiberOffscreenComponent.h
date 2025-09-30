#pragma once

// Simplified port of react-main/packages/react-reconciler/src/ReactFiberOffscreenComponent.js
// Provides minimal Offscreen visibility primitives required by the concurrent
// updates helpers.

#include <cstdint>

namespace react {

using OffscreenVisibility = std::uint8_t;

inline constexpr OffscreenVisibility OffscreenVisible = 0b001;
inline constexpr OffscreenVisibility OffscreenPassiveEffectsConnected = 0b010;

struct OffscreenInstance {
	OffscreenVisibility _visibility{OffscreenVisible};
};

} // namespace react
