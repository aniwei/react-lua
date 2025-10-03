#pragma once

#include "runtime/ReactWasmLayout.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace react::browser {

struct WasmLayout {
  std::vector<uint8_t> buffer;
  uint32_t rootOffset{0};
};

WasmLayout buildKeyedSpanLayout(const std::vector<std::pair<std::string, std::string>>& childrenSpecs);

} // namespace react::browser
