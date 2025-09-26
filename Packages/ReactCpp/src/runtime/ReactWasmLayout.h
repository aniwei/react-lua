#pragma once

#include <cstdint>

// Use 1-byte alignment to ensure C++ and JS memory layouts match exactly.
#pragma pack(push, 1)

namespace react {

// Enum to represent the type of a value in our binary format.
enum class WasmValueType : uint8_t {
  Null,
  Undefined,
  Boolean,
  Number,
  String,
  Element,
  Array,
};

// Represents a generic value. The `type` field determines which
// part of the `data` union is active.
struct WasmReactValue {
  WasmValueType type;
  union {
    bool boolValue;
    double numberValue;
    // For complex types (String, Element, Array), this will be an
    // offset into the Wasm memory block.
    uint32_t ptrValue;
  } data;
};

struct WasmReactArray {
  // Number of elements in the array.
  uint32_t length;
  // Offset to the contiguous block of `WasmReactValue` entries.
  uint32_t items_ptr;
};

// Represents a single property (key-value pair) of a React Element.
struct WasmReactProp {
  // Offset to the null-terminated string for the prop key.
  uint32_t key_ptr;
  // The value of the prop.
  WasmReactValue value;
};

// The core binary representation of a React Element.
struct WasmReactElement {
  // Offset to the null-terminated string for the element type (e.g., "div").
  uint32_t type_name_ptr;

  // Offset to the element's key (or 0 if no key).
  uint32_t key_ptr;

  // Offset to the element's ref (or 0 if no ref).
  uint32_t ref_ptr;

  // Number of properties.
  uint32_t props_count;
  // Offset to the array of WasmReactProp.
  uint32_t props_ptr;

  // Number of children.
  uint32_t children_count;
  // Offset to the array of WasmReactValue (where each value is an Element or String).
  uint32_t children_ptr;
};

} // namespace react

#pragma pack(pop)
