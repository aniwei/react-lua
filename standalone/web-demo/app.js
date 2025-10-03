import createReactRuntimeModule from './react_wasm_web_demo.js';
import { attachReactWasmToWindow } from './react-wasm-bridge.js';

const scenarios = [
  {
    id: 0,
    label: 'Initial state',
    chips: [
      { key: 'alpha', label: 'Alpha', className: 'chip' },
      { key: 'beta', label: 'Beta', className: 'chip' },
      { key: 'gamma', label: 'Gamma', className: 'chip' },
    ],
  },
  {
    id: 1,
    label: 'Update classes',
    chips: [
      { key: 'alpha', label: 'Alpha', className: 'chip' },
      { key: 'beta', label: 'Beta', className: 'chip active' },
      { key: 'gamma', label: 'Gamma', className: 'chip' },
    ],
  },
  {
    id: 2,
    label: 'Reorder items',
    chips: [
      { key: 'gamma', label: 'Gamma', className: 'chip' },
      { key: 'alpha', label: 'Alpha', className: 'chip' },
      { key: 'beta', label: 'Beta', className: 'chip active' },
    ],
  },
  {
    id: 3,
    label: 'Remove last item',
    chips: [
      { key: 'gamma', label: 'Gamma', className: 'chip' },
      { key: 'beta', label: 'Beta', className: 'chip active' },
    ],
  },
];

const updateLog = (message) => {
  const logElement = document.getElementById('log');
  if (logElement) {
    logElement.textContent = message;
  }
};

createReactRuntimeModule()
  .then((Module) => {
    const ReactWasm = attachReactWasmToWindow(Module);
    const { render, jsx: createElement } = ReactWasm;

    const root = document.getElementById('app-root');
    if (!root) {
      throw new Error('Missing #app-root container');
    }

    const App = ({ scenario }) => {
      const chips = scenario.chips.map((chip) =>
        createElement('span', { className: chip.className, children: chip.label }, chip.key),
      );

      return createElement('div', { className: 'chip-container', children: chips });
    };

    let currentScenario = scenarios[0];

    const rerender = () => {
      render(createElement(App, { scenario: currentScenario }), root);
      updateLog(`Rendered: ${currentScenario.label}`);
    };

    document.querySelectorAll('[data-step]').forEach((button) => {
      const scenarioId = Number(button.dataset.step || '0');
      button.addEventListener('click', () => {
        const next = scenarios.find((item) => item.id === scenarioId);
        if (!next) {
          return;
        }
        currentScenario = next;
        rerender();
      });
    });

    const clearButton = document.getElementById('clear');
    if (clearButton) {
      clearButton.addEventListener('click', () => {
        ReactWasm.clear(root);
        updateLog('Cleared root container.');
      });
    }

    updateLog('Wasm runtime ready. Rendering initial state…');
    rerender();
  })
  .catch((error) => {
    console.error('Failed to load Wasm module', error);
    updateLog('Failed to load Wasm module.');
  });
