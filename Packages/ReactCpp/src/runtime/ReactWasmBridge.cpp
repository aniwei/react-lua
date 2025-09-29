#include "ReactWasmBridge.h"
#include "ReactHostInterface.h"
#include "ReactRuntime.h"
#include "ReactWasmLayout.h"
#include "jsi/jsi.h"
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace jsi = facebook::jsi;

namespace react {

// Assume Wasm memory is accessible as a global byte array for simplicity.
// In a real scenario, this would be managed by the Wasm runtime.
uint8_t* __wasm_memory_buffer = nullptr;

// --- Binary to JSI Deserializer ---

// Forward declaration
jsi::Value convertWasmLayoutToJsi(
  jsi::Runtime& rt,
  uint32_t baseOffset,
  const WasmReactValue& wasmValue);

// Helper to get a pointer into the Wasm memory
template<typename T>
T* getPointer(uint32_t baseOffset, uint32_t offset) {
  return reinterpret_cast<T*>(__wasm_memory_buffer + baseOffset + offset);
}

jsi::Value convertWasmElementToJsi(
  jsi::Runtime& rt,
  uint32_t baseOffset,
  uint32_t elementOffset) {
  WasmReactElement* element = getPointer<WasmReactElement>(baseOffset, elementOffset);
  
  // Create a JSI object for the element
  jsi::Object jsiElement(rt);

  // Set $$typeof (使用字符串占位，后续可替换为真正的 Symbol 写入)
  jsiElement.setProperty(
    rt,
    jsi::PropNameID::forUtf8(rt, "$$typeof"),
    jsi::Value(jsi::String::createFromUtf8(rt, "react.element")));

  // Set type
  const char* typeName = getPointer<const char>(baseOffset, element->type_name_ptr);
  jsiElement.setProperty(rt, "type", jsi::String::createFromUtf8(rt, typeName));

  // Set key
  jsi::Value keyValue = element->key_ptr == 0
    ? jsi::Value::null()
    : jsi::Value(jsi::String::createFromUtf8(rt, getPointer<const char>(baseOffset, element->key_ptr)));
  jsiElement.setProperty(rt, "key", keyValue);

  // Set ref
  jsi::Value refValue = element->ref_ptr == 0
    ? jsi::Value::null()
    : jsi::Value(jsi::String::createFromUtf8(rt, getPointer<const char>(baseOffset, element->ref_ptr)));
  jsiElement.setProperty(rt, "ref", refValue);

  // Set props
  jsi::Object jsiProps(rt);
  if (element->props_count > 0 && element->props_ptr != 0) {
    WasmReactProp* props = getPointer<WasmReactProp>(baseOffset, element->props_ptr);
    for (uint32_t i = 0; i < element->props_count; ++i) {
      if (props[i].key_ptr == 0) {
        continue;
      }
      const char* propKey = getPointer<const char>(baseOffset, props[i].key_ptr);
      jsi::Value propValue = convertWasmLayoutToJsi(rt, baseOffset, props[i].value);
      jsiProps.setProperty(rt, propKey, propValue);
    }
  }

  // Set children
  if (element->children_count == 0 || element->children_ptr == 0) {
    jsiProps.setProperty(rt, "children", jsi::Value::undefined());
  } else if (element->children_count == 1) {
    WasmReactValue* child = getPointer<WasmReactValue>(baseOffset, element->children_ptr);
    jsi::Value childValue = convertWasmLayoutToJsi(rt, baseOffset, child[0]);
    jsiProps.setProperty(rt, "children", childValue);
  } else {
    jsi::Array jsiChildren(rt, element->children_count);
    WasmReactValue* children = getPointer<WasmReactValue>(baseOffset, element->children_ptr);
    for (uint32_t i = 0; i < element->children_count; ++i) {
      jsi::Value childValue = convertWasmLayoutToJsi(rt, baseOffset, children[i]);
      jsiChildren.setValueAtIndex(rt, i, childValue);
    }
    jsiProps.setProperty(rt, "children", jsi::Value(std::move(jsiChildren)));
  }

  jsiElement.setProperty(rt, "props", jsiProps);

  return jsi::Value(std::move(jsiElement));
}

jsi::Value convertWasmLayoutToJsi(jsi::Runtime& rt, uint32_t baseOffset, const WasmReactValue& wasmValue) {
  switch (wasmValue.type) {
    case WasmValueType::Null:
      return jsi::Value::null();
    case WasmValueType::Undefined:
      return jsi::Value::undefined();
    case WasmValueType::Boolean:
      return jsi::Value(wasmValue.data.boolValue);
    case WasmValueType::Number:
      return jsi::Value(wasmValue.data.numberValue);
    case WasmValueType::String:
      return jsi::Value(jsi::String::createFromUtf8(rt, getPointer<const char>(baseOffset, wasmValue.data.ptrValue)));
    case WasmValueType::Element: {
      return convertWasmElementToJsi(rt, baseOffset, wasmValue.data.ptrValue);
    }
    case WasmValueType::Array: {
      if (wasmValue.data.ptrValue == 0) {
        return jsi::Value::undefined();
      }
      WasmReactArray* wasmArray = getPointer<WasmReactArray>(baseOffset, wasmValue.data.ptrValue);
      jsi::Array jsiArray(rt, wasmArray->length);
      if (wasmArray->length > 0 && wasmArray->items_ptr != 0) {
        WasmReactValue* items = getPointer<WasmReactValue>(baseOffset, wasmArray->items_ptr);
        for (uint32_t i = 0; i < wasmArray->length; ++i) {
          jsi::Value itemValue = convertWasmLayoutToJsi(rt, baseOffset, items[i]);
          jsiArray.setValueAtIndex(rt, i, itemValue);
        }
      }
      return jsi::Value(std::move(jsiArray));
    }
    default:
      return jsi::Value::undefined();
  }
}

// --- Wasm Bridge Entry Points ---

static ReactRuntime* G_ReactRuntime = nullptr;
static jsi::Runtime* G_JsiRuntime = nullptr;
static std::shared_ptr<HostInterface> G_HostInterface = std::make_shared<HostInterface>();
static std::unordered_map<uint32_t, std::shared_ptr<ReactDOMInstance>> G_RootContainers;

void react_set_host_interface(std::shared_ptr<HostInterface> hostInterface) {
  if (!hostInterface) {
    hostInterface = std::make_shared<HostInterface>();
  }
  G_HostInterface = std::move(hostInterface);
  if (!G_ReactRuntime) {
    return;
  }
  G_ReactRuntime->setHostInterface(G_HostInterface);
  if (G_JsiRuntime) {
    G_ReactRuntime->bindHostInterface(*G_JsiRuntime);
  }
}

namespace {

std::shared_ptr<ReactDOMInstance> resolveRootContainer(uint32_t rootContainerId) {
  auto it = G_RootContainers.find(rootContainerId);
  if (it == G_RootContainers.end()) {
    return nullptr;
  }
  return it->second;
}

} // namespace

extern "C" {
  // Called by JS to initialize the runtime.
  void react_init(void* memory_buffer) {
    __wasm_memory_buffer = static_cast<uint8_t*>(memory_buffer);
    static ReactRuntime s_runtime;
    if (G_ReactRuntime == nullptr) {
      G_ReactRuntime = &s_runtime;
    }
    if (G_ReactRuntime) {
      G_ReactRuntime->setHostInterface(G_HostInterface);
      if (G_JsiRuntime) {
        G_ReactRuntime->bindHostInterface(*G_JsiRuntime);
      }
    }
  }

  // The main render entry point called from JS.
  void react_render(uint32_t rootElementOffset, uint32_t rootContainerId) {
    if (!G_ReactRuntime || !G_JsiRuntime) {
      return;
    }
    std::shared_ptr<ReactDOMInstance> rootContainer = resolveRootContainer(rootContainerId);
    if (!rootContainer) {
      return;
    }
    G_ReactRuntime->renderRootSync(*G_JsiRuntime, rootElementOffset, rootContainer);
  }

  void react_hydrate(uint32_t rootElementOffset, uint32_t rootContainerId) {
    if (!G_ReactRuntime || !G_JsiRuntime) {
      return;
    }
    std::shared_ptr<ReactDOMInstance> rootContainer = resolveRootContainer(rootContainerId);
    if (!rootContainer) {
      return;
    }
    G_ReactRuntime->hydrateRoot(*G_JsiRuntime, rootElementOffset, rootContainer);
  }

  // Memory allocation functions to be called from JS.
  void* react_malloc(size_t size) {
    return malloc(size);
  }

  void react_free(void* ptr) {
    free(ptr);
  }

  void react_register_root_container(uint32_t rootContainerId, ReactDOMInstance* instance) {
    if (instance == nullptr) {
      G_RootContainers.erase(rootContainerId);
      return;
    }

    G_RootContainers[rootContainerId] = std::shared_ptr<ReactDOMInstance>(instance, [](ReactDOMInstance*) {
      // The host environment owns the lifetime; do nothing on release.
    });
  }

  void react_clear_root_container(uint32_t rootContainerId) {
    G_RootContainers.erase(rootContainerId);
  }

  void react_attach_jsi_runtime(jsi::Runtime* runtime) {
    G_JsiRuntime = runtime;
    if (runtime != nullptr && G_ReactRuntime != nullptr) {
      G_ReactRuntime->setHostInterface(G_HostInterface);
      G_ReactRuntime->bindHostInterface(*runtime);
    }
  }

  void react_attach_runtime(ReactRuntime* runtime) {
    G_ReactRuntime = runtime;
    if (!G_ReactRuntime) {
      return;
    }
    G_ReactRuntime->setHostInterface(G_HostInterface);
    if (G_JsiRuntime) {
      G_ReactRuntime->bindHostInterface(*G_JsiRuntime);
    }
  }

  void react_reset_runtime() {
    if (G_ReactRuntime) {
      G_ReactRuntime->resetWorkLoop();
    }
    G_ReactRuntime = nullptr;
    G_JsiRuntime = nullptr;
    G_RootContainers.clear();
  }
}

} // namespace react
