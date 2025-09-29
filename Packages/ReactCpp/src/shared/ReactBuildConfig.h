#pragma once

#ifndef REACTCPP_ENABLE_DEV
#define REACTCPP_ENABLE_DEV 1
#endif

namespace react {

inline constexpr bool kDevBuild = REACTCPP_ENABLE_DEV != 0;

} // namespace react
