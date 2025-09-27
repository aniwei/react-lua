#include "ReactWasmLayout.h"
#include "jsi/jsi.h"

namespace react {

facebook::jsi::Value convertWasmLayoutToJsi(
    facebook::jsi::Runtime&,
    uint32_t,
    const WasmReactValue&) {
  return facebook::jsi::Value::undefined();
}

} // namespace react
