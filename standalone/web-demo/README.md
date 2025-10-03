# React C++ Web Demo

This folder contains a minimal WebAssembly harness that connects the C++ `ReactRuntime` to real browser DOM nodes. It uses Emscripten to compile the runtime plus a bespoke `BrowserHostInterface` that translates update payloads into DOM mutations.

## Quick start

1. Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) and activate the environment so `emcmake`/`em++` are on your `PATH`.
2. Configure and build the Wasm module:

   ```bash
   emcmake cmake -S standalone/web-demo -B build/web-demo -G Ninja
   cmake --build build/web-demo
   ```

   The build emits `react_wasm_web_demo.js` and `react_wasm_web_demo.wasm` alongside the HTML/JS assets.
3. Serve the directory (for example `python3 -m http.server 9000 --directory standalone/web-demo`) and open <http://localhost:9000>.
4. Use the control buttons to trigger prop updates, reorders, and deletions. Inspect the DOM to verify keyed reconciliation.

When the Wasm module loads the shared [`react-wasm-bridge.js`](./react-wasm-bridge.js) helper attaches a `ReactWasm` runtime to `window` with a React-like surface:

```js
const { jsx, render, Fragment } = window.ReactWasm;

const Chip = ({ label, active }) =>
   jsx(
      'span',
      { className: active ? 'chip active' : 'chip', children: label },
      label,
   );

const App = () => (
   jsx('div', {
      className: 'chip-container',
      children: [
         jsx(Chip, { label: 'Alpha', active: false }),
         jsx(Chip, { label: 'Beta', active: true }),
         jsx(Chip, { label: 'Gamma', active: false }),
      ],
   })
);

render(jsx(App, {}), document.getElementById('app-root'));
```

Any tree you build with `jsx`/`jsxs` is serialised to the `WasmReactLayout` format and diffed by the C++ runtime before mutating the DOM.

## Bundler usage

You can reuse the same bridge outside of this demo. Import the helper, create the Wasm module with Emscripten’s async factory, and wire it into your toolchain:

```js
import createReactRuntimeModule from './react_wasm_web_demo.js';
import { createReactWasmBridge } from './react-wasm-bridge.js';

const Module = await createReactRuntimeModule();
const ReactWasm = createReactWasmBridge(Module);

const App = ReactWasm.jsx('div', { children: 'Hello from Wasm!' });
ReactWasm.render(App, document.getElementById('root'));
```

For automatic JSX transforms point your bundler at the bridge. With TypeScript you can alias it in `tsconfig.json`:

```json
{
   "compilerOptions": {
      "jsx": "react-jsx",
      "jsxImportSource": "@react-wasm",
      "paths": {
         "@react-wasm/jsx-runtime": ["./standalone/web-demo/react-wasm-bridge.js"],
         "@react-wasm/jsx-dev-runtime": ["./standalone/web-demo/react-wasm-bridge.js"]
      }
   }
}
```

The same alias works with Babel or SWC via `module-resolver`/`tsconfig-paths`. Once configured, bundler-generated JSX will call the bridge’s `jsx/jsxs/Fragment` exports while the `createReactWasmBridge` return value gives you `render`, `clear`, and helpers.

More context and future work ideas live in [`docs/react-wasm-web-demo.md`](../../docs/react-wasm-web-demo.md).
