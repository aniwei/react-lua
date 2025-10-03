# React C++ WebAssembly Demo

This guide wires the C++ `ReactRuntime` into a real browser DOM. The example lives in `standalone/web-demo` and exports a tiny keyed list renderer that exercises the incremental reconciliation logic implemented in C++.

## What the demo does

* Builds the core `ReactRuntime` and helpers (`BrowserHostInterface`) to real DOM nodes via `emscripten::val`.
* Serialises simple element trees with the same `WasmReactLayout` structs used by the runtime.
* Shows keyed reuse, prop updates, reorders, and deletions when you click through the UI controls.

The exported C functions:

| Symbol | Purpose |
| - | - |
| `react_browser_bootstrap(const char* elementId)` | Creates/attaches a root container for the element with the provided id and returns an opaque handle. |
| `react_browser_render_step(uint32_t handle, int step)` | Renders one of four predefined keyed layouts (initial, update, reorder, delete). Useful for smoke testing. |
| `react_browser_render_layout(uint32_t handle, uint32_t ptr, uint32_t rootOffset)` | Consumes a `WasmReactElement` tree written into Wasm memory and performs a full diff/mutation pass. |
| `react_browser_clear(uint32_t handle)` | Clears the root container by issuing an empty render. |

The browser harness reuses `react-wasm-bridge.js` to build a React-like surface on top of these exports:

```js
import createReactRuntimeModule from './react_wasm_web_demo.js';
import { attachReactWasmToWindow } from './react-wasm-bridge.js';

const Module = await createReactRuntimeModule();
const { jsx, jsxs, Fragment, render } = attachReactWasmToWindow(Module);

const demoChips = [
	{ key: 'alpha', label: 'Alpha', className: 'chip' },
	{ key: 'beta', label: 'Beta', className: 'chip active' },
];

const App = ({ chips }) => jsx('div', {
	className: 'chip-container',
	children: chips.map((chip) =>
		jsx('span', { className: chip.className, children: chip.label }, chip.key),
	),
});

render(jsx(App, { chips: demoChips }), document.getElementById('app-root'));
```

Every JSX tree is serialised into the `WasmReactLayout` format inside JavaScript, copied into Wasm memory, and handed to `react_browser_render_layout` for reconciliation. The bridge also exports `{ jsx, jsxs, jsxDEV, Fragment }` so bundlers can target it as a custom JSX runtime via `jsxImportSource`, covering both production and development transforms.

## Prerequisites

* [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) – the environment should expose `emcmake`, `emconfigure`, and the `em++` toolchain in your `$PATH`.
* CMake ≥ 3.13 and Ninja (recommended for faster builds).
* A static file server (the examples below use Python's `http.server`).

## Build steps

```bash
# 1. Configure the demo with the Emscripten toolchain
emcmake cmake -S standalone/web-demo -B build/web-demo -G Ninja

# 2. Compile the WebAssembly module and companion JS loader
cmake --build build/web-demo
```

The build emits two files inside `build/web-demo`: `react_wasm_web_demo.wasm` and `react_wasm_web_demo.js`. Copy both into `standalone/web-demo/` (the build directory already writes them there if you use the defaults above).

## Run the demo locally

```bash
# From the repository root
python3 -m http.server 9000 --directory standalone/web-demo
```

Navigate to <http://localhost:9000>. The page loads the Wasm module, bootstraps a root container, and renders a three-pill list. Use the buttons to observe how the DOM updates without recreating nodes:

1. **Initial state** – materialises three keyed `<span>` elements.
2. **Update classes** – updates only the "beta" chip's class without remounting siblings.
3. **Reorder items** – reuses the same DOM nodes while shifting their positions.
4. **Remove last item** – issues the deletion mutation for the trailing node.
5. **Clear root** – removes all children.

Open your browser console to see timing logs (added via the JS harness) and inspect the DOM to confirm the keyed reconciliation behaviour.

## Implementation notes

* `BrowserHostInterface` subclasses `HostInterface` and mirrors the DOM host config from upstream React. It translates payload diffs into DOM mutations via `emscripten::val`.
* The demo uses the lightweight `TestRuntime` JSI stub that already powers the C++ unit tests. No external JS engine is required.
* Layout data can be produced either in C++ (see `WasmLayoutBuilder`) or directly in JavaScript via the serializer shared by `react-wasm-bridge.js`, guaranteeing the memory format matches the runtime expectations.
* The build links the full `react_cpp_src` library plus the demo-specific source files; `target_link_options` exports only the small set of functions required by `app.js`.

## Next steps

* Swap the canned layouts for a JSON-to-layout encoder on the JavaScript side to drive arbitrary trees.
* Bridge event props in `BrowserHostInterface` so interactions can flow back into the runtime.
* Integrate the Wasm module with the upstream React test harness for parity verification.