#pragma once

#include <cstdint>

namespace react {

enum class RootTag : std::uint8_t {
	LegacyRoot = 0,
	ConcurrentRoot = 1,
};

} // namespace react
