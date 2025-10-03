#ifdef __EMSCRIPTEN__

#include "BrowserHostInterface.h"
#include "WasmLayoutBuilder.h"

#include "runtime/ReactRuntime.h"
#include "runtime/ReactWasmBridge.h"
#include "react-dom/client/ReactDOMComponent.h"
#include "TestRuntime.h"

#include <emscripten/val.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace react::browser {

namespace demo {
std::shared_ptr<BrowserHostInterface> g_hostInterface;
ReactRuntime g_runtime;
react::test::TestRuntime g_jsiRuntime;
std::unordered_map<uint32_t, std::shared_ptr<ReactDOMInstance>> g_roots;
uint32_t g_nextRootId = 1;

void ensureInitialized() {
  if (!g_hostInterface) {
    g_hostInterface = std::make_shared<BrowserHostInterface>();
  }
  g_runtime.setHostInterface(g_hostInterface);
  g_hostInterface->attachRuntime(g_jsiRuntime);
}

std::shared_ptr<ReactDOMInstance> getRoot(uint32_t handle) {
  auto it = g_roots.find(handle);
  if (it == g_roots.end()) {
    return nullptr;
  }
  return it->second;
}

void renderLayout(
    uint32_t rootHandle,
    const std::vector<std::pair<std::string, std::string>>& specs) {
  auto root = getRoot(rootHandle);
  if (!root) {
    return;
  }

  WasmLayout layout = buildKeyedSpanLayout(specs);
  __wasm_memory_buffer = layout.buffer.data();
  g_runtime.renderRootSync(g_jsiRuntime, layout.rootOffset, root);
  __wasm_memory_buffer = nullptr;
}

std::vector<std::pair<std::string, std::string>> stableList() {
  return {
      {"alpha", "chip"},
      {"beta", "chip"},
      {"gamma", "chip"}};
}

std::vector<std::pair<std::string, std::string>> classUpdateList() {
  return {
      {"alpha", "chip"},
      {"beta", "chip active"},
      {"gamma", "chip"}};
}

std::vector<std::pair<std::string, std::string>> reorderList() {
  return {
      {"gamma", "chip"},
      {"alpha", "chip"},
      {"beta", "chip active"}};
}

std::vector<std::pair<std::string, std::string>> removalList() {
  return {
      {"gamma", "chip"},
      {"beta", "chip active"}};
}

} // namespace demo

} // namespace react::browser

extern "C" {

uint32_t react_browser_bootstrap(const char* rootElementId) {
  react::browser::demo::ensureInitialized();
  std::string id = rootElementId ? std::string(rootElementId) : std::string("react-root");
  auto container = react::browser::demo::g_hostInterface->createRootContainer(id);
  if (!container) {
    return 0;
  }
  const uint32_t handle = react::browser::demo::g_nextRootId++;
  react::browser::demo::g_roots.emplace(handle, container);
  return handle;
}

void react_browser_render_step(uint32_t rootHandle, int step) {
  if (step <= 0) {
    react::browser::demo::renderLayout(rootHandle, react::browser::demo::stableList());
  } else if (step == 1) {
    react::browser::demo::renderLayout(rootHandle, react::browser::demo::classUpdateList());
  } else if (step == 2) {
    react::browser::demo::renderLayout(rootHandle, react::browser::demo::reorderList());
  } else if (step >= 3) {
    react::browser::demo::renderLayout(rootHandle, react::browser::demo::removalList());
  }
}

void react_browser_render_layout(uint32_t rootHandle, uint32_t layoutPtr, uint32_t rootElementOffset) {
  auto root = react::browser::demo::getRoot(rootHandle);
  if (!root) {
    return;
  }

  if (layoutPtr == 0 || rootElementOffset == 0) {
    __wasm_memory_buffer = nullptr;
    react::browser::demo::g_runtime.renderRootSync(react::browser::demo::g_jsiRuntime, 0, root);
    return;
  }

  auto bufferPtr = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(layoutPtr));
  __wasm_memory_buffer = bufferPtr;
  react::browser::demo::g_runtime.renderRootSync(react::browser::demo::g_jsiRuntime, rootElementOffset, root);
  __wasm_memory_buffer = nullptr;
}

void react_browser_clear(uint32_t rootHandle) {
  auto root = react::browser::demo::getRoot(rootHandle);
  if (!root) {
    return;
  }
  __wasm_memory_buffer = nullptr;
  react::browser::demo::g_runtime.renderRootSync(react::browser::demo::g_jsiRuntime, 0, root);
}

} // extern "C"

#endif // __EMSCRIPTEN__
