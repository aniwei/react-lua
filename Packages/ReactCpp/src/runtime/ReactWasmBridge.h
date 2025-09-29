#pragma once

#include <cstddef>
#include <cstdint>

namespace facebook {
namespace jsi {
class Runtime;
class Value;
} // namespace jsi
} // namespace facebook

namespace react {

class ReactDOMInstance;
class ReactRuntime;
struct WasmReactValue;

extern uint8_t* __wasm_memory_buffer;
facebook::jsi::Value convertWasmLayoutToJsi(
  facebook::jsi::Runtime& rt,
  uint32_t baseOffset,
  const WasmReactValue& wasmValue);

extern "C" {
  void react_init(void* memory_buffer);
  void react_render(uint32_t rootElementOffset, uint32_t rootContainerId);
  void react_hydrate(uint32_t rootElementOffset, uint32_t rootContainerId);
  void* react_malloc(size_t size);
  void react_free(void* ptr);
  void react_register_root_container(uint32_t rootContainerId, ReactDOMInstance* instance);
  void react_clear_root_container(uint32_t rootContainerId);
  void react_attach_jsi_runtime(facebook::jsi::Runtime* runtime);
  void react_attach_runtime(ReactRuntime* runtime);
  void react_reset_runtime();
}

} // namespace react
