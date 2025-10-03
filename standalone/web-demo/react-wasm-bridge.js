export const REACT_ELEMENT_TYPE = Symbol.for('react.element');
export const REACT_FRAGMENT_TYPE = Symbol.for('react.fragment');

const WasmValueType = {
  Null: 0,
  Undefined: 1,
  Boolean: 2,
  Number: 3,
  String: 4,
  Element: 5,
  Array: 6,
};

export function jsx(type, config = {}, maybeKey, ...children) {
  const props = { ...config };
  let key = null;

  if (maybeKey !== undefined && maybeKey !== null) {
    key = String(maybeKey);
  }

  if (props.key != null) {
    key = String(props.key);
    delete props.key;
  }

  if (children.length === 1) {
    props.children = children[0];
  } else if (children.length > 1) {
    props.children = children;
  }

  return {
    $$typeof: REACT_ELEMENT_TYPE,
    type,
    key,
    props,
  };
}

export const jsxs = jsx;
export const jsxDEV = jsx;
export const Fragment = REACT_FRAGMENT_TYPE;

class WasmLayoutBuilder {
  constructor() {
    this.bytes = [0];
    this.stringCache = new Map();
  }

  appendString(value) {
    const normalized = String(value);
    if (this.stringCache.has(normalized)) {
      return this.stringCache.get(normalized);
    }
    const offset = this.bytes.length;
    for (let i = 0; i < normalized.length; i += 1) {
      this.bytes.push(normalized.charCodeAt(i) & 0xff);
    }
    this.bytes.push(0);
    this.stringCache.set(normalized, offset);
    return offset;
  }

  writeUint32(value) {
    const v = value >>> 0;
    this.bytes.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  }

  writeDouble(value) {
    const buffer = new ArrayBuffer(8);
    new DataView(buffer).setFloat64(0, value, true);
    const view = new Uint8Array(buffer);
    for (let i = 0; i < view.length; i += 1) {
      this.bytes.push(view[i]);
    }
  }

  writeValue(spec) {
    this.bytes.push(spec.type);
    if (spec.type === WasmValueType.Boolean) {
      this.bytes.push(spec.value ? 1 : 0);
      for (let i = 1; i < 8; i += 1) {
        this.bytes.push(0);
      }
      return;
    }

    if (spec.type === WasmValueType.Number) {
      this.writeDouble(spec.value);
      return;
    }

    const pointer = spec.pointer >>> 0;
    this.bytes.push(pointer & 0xff);
    this.bytes.push((pointer >> 8) & 0xff);
    this.bytes.push((pointer >> 16) & 0xff);
    this.bytes.push((pointer >> 24) & 0xff);
    for (let i = 4; i < 8; i += 1) {
      this.bytes.push(0);
    }
  }

  appendProp(keyOffset, spec) {
    const offset = this.bytes.length;
    this.writeUint32(keyOffset);
    this.writeValue(spec);
    return offset;
  }

  appendProps(propSpecs) {
    if (propSpecs.length === 0) {
      return 0;
    }
    let firstOffset = 0;
    for (let index = 0; index < propSpecs.length; index += 1) {
      const entry = propSpecs[index];
      const offset = this.appendProp(entry.keyOffset, entry.valueSpec);
      if (index === 0) {
        firstOffset = offset;
      }
    }
    return firstOffset;
  }

  appendChildren(childSpecs) {
    if (childSpecs.length === 0) {
      return 0;
    }
    let firstOffset = 0;
    for (let index = 0; index < childSpecs.length; index += 1) {
      const offset = this.bytes.length;
      this.writeValue(childSpecs[index]);
      if (index === 0) {
        firstOffset = offset;
      }
    }
    return firstOffset;
  }

  appendElement(typeOffset, keyOffset, propsCount, propsOffset, childrenCount, childrenOffset) {
    const offset = this.bytes.length;
    this.writeUint32(typeOffset);
    this.writeUint32(keyOffset);
    this.writeUint32(0); // ref_ptr
    this.writeUint32(propsCount);
    this.writeUint32(propsOffset);
    this.writeUint32(childrenCount);
    this.writeUint32(childrenOffset);
    return offset;
  }

  toUint8Array() {
    return Uint8Array.from(this.bytes);
  }
}

function normalizeChildren(children) {
  if (children === undefined || children === null) {
    return [];
  }
  if (Array.isArray(children)) {
    return children;
  }
  return [children];
}

function resolveElement(element) {
  if (element === null || element === undefined || typeof element === 'boolean') {
    return null;
  }
  if (typeof element === 'string' || typeof element === 'number') {
    return { kind: 'text', value: element };
  }
  if (Array.isArray(element)) {
    const flattened = [];
    for (const child of element) {
      const resolved = resolveElement(child);
      if (resolved === null) {
        continue;
      }
      if (Array.isArray(resolved)) {
        flattened.push(...resolved);
      } else {
        flattened.push(resolved);
      }
    }
    return flattened;
  }
  if (typeof element === 'object' && element.$$typeof === REACT_ELEMENT_TYPE) {
    if (typeof element.type === 'function') {
      return resolveElement(element.type(element.props ?? {}));
    }
    if (element.type === REACT_FRAGMENT_TYPE) {
      return resolveElement(normalizeChildren(element.props?.children));
    }
    if (typeof element.type === 'string') {
      const props = { ...(element.props ?? {}) };
      const childValues = normalizeChildren(props.children);
      delete props.children;

      const resolvedChildren = [];
      for (const child of childValues) {
        const resolved = resolveElement(child);
        if (resolved === null) {
          continue;
        }
        if (Array.isArray(resolved)) {
          resolvedChildren.push(...resolved);
        } else {
          resolvedChildren.push(resolved);
        }
      }

      return {
        kind: 'host',
        type: element.type,
        key: element.key,
        props,
        children: resolvedChildren,
      };
    }
  }

  throw new Error(`Unsupported element type: ${String(element)}`);
}

function ensureHostRoot(resolved) {
  if (resolved === null) {
    return null;
  }
  if (Array.isArray(resolved)) {
    if (resolved.length === 0) {
      return null;
    }
    if (resolved.length === 1 && resolved[0].kind === 'host') {
      return resolved[0];
    }
    return {
      kind: 'host',
      type: 'div',
      key: null,
      props: {},
      children: resolved,
    };
  }
  if (resolved.kind === 'text') {
    return {
      kind: 'host',
      type: 'span',
      key: null,
      props: {},
      children: [resolved],
    };
  }
  return resolved;
}

function encodeHostElement(builder, node) {
  const typeOffset = builder.appendString(node.type);
  const keyOffset = node.key != null ? builder.appendString(node.key) : 0;

  const propEntries = [];
  for (const [rawKey, rawValue] of Object.entries(node.props)) {
    if (rawValue === null || rawValue === undefined) {
      continue;
    }
    const keyOffsetEntry = builder.appendString(rawKey);
    if (typeof rawValue === 'string') {
      const strOffset = builder.appendString(rawValue);
      propEntries.push({
        keyOffset: keyOffsetEntry,
        valueSpec: { type: WasmValueType.String, pointer: strOffset },
      });
      continue;
    }
    if (typeof rawValue === 'number') {
      propEntries.push({
        keyOffset: keyOffsetEntry,
        valueSpec: { type: WasmValueType.Number, value: rawValue },
      });
      continue;
    }
    if (typeof rawValue === 'boolean') {
      propEntries.push({
        keyOffset: keyOffsetEntry,
        valueSpec: { type: WasmValueType.Boolean, value: rawValue },
      });
      continue;
    }

    throw new Error(`Unsupported prop ${rawKey} on <${node.type}>`);
  }

  const childValues = [];
  for (const child of node.children) {
    if (child.kind === 'text') {
      if (typeof child.value === 'number') {
        childValues.push({ type: WasmValueType.Number, value: child.value });
      } else {
        const strOffset = builder.appendString(child.value);
        childValues.push({ type: WasmValueType.String, pointer: strOffset });
      }
      continue;
    }

    if (child.kind === 'host') {
      const childOffset = encodeHostElement(builder, child);
      childValues.push({ type: WasmValueType.Element, pointer: childOffset });
      continue;
    }

    throw new Error('Unsupported child node.');
  }

  const propsOffset = builder.appendProps(propEntries);
  const childrenOffset = builder.appendChildren(childValues);
  const elementOffset = builder.appendElement(
    typeOffset,
    keyOffset,
    propEntries.length,
    propsOffset,
    childValues.length,
    childrenOffset,
  );

  return elementOffset;
}

export function createReactWasmBridge(Module) {
  const roots = new Map();
  let idSeed = 1;

  const toCString = (text) => {
    const length = Module.lengthBytesUTF8(text) + 1;
    const pointer = Module._malloc(length);
    Module.stringToUTF8(text, pointer, length);
    return pointer;
  };

  const ensureHandle = (container) => {
    if (!(container instanceof HTMLElement)) {
      throw new Error('render(...) expects a DOM element as the container.');
    }
    if (!container.id) {
      container.id = `react-wasm-root-${idSeed}`;
      idSeed += 1;
    }
    if (roots.has(container)) {
      return roots.get(container);
    }
    const ptr = toCString(container.id);
    const handle = Module._react_browser_bootstrap(ptr);
    Module._free(ptr);
    if (!handle) {
      throw new Error(`Failed to bootstrap root for #${container.id}`);
    }
    roots.set(container, handle);
    return handle;
  };

  const render = (element, container) => {
    const handle = ensureHandle(container);
    const resolved = ensureHostRoot(resolveElement(element));
    if (resolved === null) {
      Module._react_browser_render_layout(handle, 0, 0);
      return;
    }

    const builder = new WasmLayoutBuilder();
    const rootOffset = encodeHostElement(builder, resolved);
    const buffer = builder.toUint8Array();
    const pointer = Module._malloc(buffer.length);
    Module.HEAPU8.set(buffer, pointer);
    Module._react_browser_render_layout(handle, pointer, rootOffset);
    Module._free(pointer);
  };

  const clear = (container) => {
    const handle = ensureHandle(container);
    Module._react_browser_render_layout(handle, 0, 0);
  };

  const destroy = (container) => {
    if (!roots.has(container)) {
      return;
    }
    const handle = roots.get(container);
    Module._react_browser_render_layout(handle, 0, 0);
    roots.delete(container);
  };

  return {
    jsx,
    jsxs,
    Fragment: REACT_FRAGMENT_TYPE,
    render,
    clear,
    destroy,
    bootstrap: ensureHandle,
    _roots: roots,
  };
}

export function attachReactWasmToWindow(Module, target = typeof window !== 'undefined' ? window : undefined) {
  const bridge = createReactWasmBridge(Module);
  if (target) {
    target.ReactWasm = bridge;
  }
  return bridge;
}

export default attachReactWasmToWindow;
