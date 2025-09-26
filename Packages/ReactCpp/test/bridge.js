'use strict';

const encoder = new TextEncoder();

const WasmValueType = {
  Null: 0,
  Undefined: 1,
  Boolean: 2,
  Number: 3,
  String: 4,
  Element: 5,
  Array: 6,
};

const SIZE = {
  VALUE_PAYLOAD: 8,
  VALUE: 9, // 1 byte tag + 8 byte union payload
  PROP: 13, // uint32 key offset + WasmReactValue
  ELEMENT: 28,
  ARRAY: 8,
};

const REACT_ELEMENT_TYPE = (function resolveReactElementType() {
  if (typeof Symbol === 'function' && Symbol.for) {
    return Symbol.for('react.element');
  }
  return 'react.element';
})();

function isReactElement(value) {
  if (!value || typeof value !== 'object') {
    return false;
  }
  const marker = value.$$typeof;
  if (marker === REACT_ELEMENT_TYPE) {
    return true;
  }
  if (typeof marker === 'string' && marker === 'react.element') {
    return true;
  }
  return value.type != null && value.props != null;
}

function resolveElementTypeName(type) {
  if (typeof type === 'string') {
    return type;
  }
  if (typeof type === 'function') {
    return type.displayName || type.name || 'anonymous';
  }
  if (typeof type === 'object' && type != null && typeof type.toString === 'function') {
    return type.toString();
  }
  throw new TypeError(`Unsupported element type: ${typeof type}`);
}

function collectChildren(children, bucket) {
  if (children === undefined) {
    return;
  }
  if (Array.isArray(children)) {
    for (const child of children) {
      collectChildren(child, bucket);
    }
    return;
  }
  bucket.push(children);
}

function getValueType(value) {
  if (value === null) {
    return WasmValueType.Null;
  }
  if (value === undefined) {
    return WasmValueType.Undefined;
  }
  if (typeof value === 'boolean') {
    return WasmValueType.Boolean;
  }
  if (typeof value === 'number') {
    return WasmValueType.Number;
  }
  if (typeof value === 'string') {
    return WasmValueType.String;
  }
  if (Array.isArray(value)) {
    return WasmValueType.Array;
  }
  if (isReactElement(value)) {
    return WasmValueType.Element;
  }
  throw new TypeError(`Unsupported prop/child value type: ${typeof value}`);
}

function measureString(value) {
  return encoder.encode(String(value)).length + 1; // include null terminator
}

function measureArray(values) {
  let size = SIZE.ARRAY;
  if (values.length > 0) {
    size += SIZE.VALUE * values.length;
    for (const item of values) {
      size += measureValue(item);
    }
  }
  return size;
}

function measureElement(element) {
  if (!isReactElement(element)) {
    throw new TypeError('measureElement expects a React element-like object.');
  }
  const props = element.props || {};
  const entries = Object.entries(props).filter(([key]) => key !== 'children');
  const children = [];
  collectChildren(props.children, children);

  let size = SIZE.ELEMENT;
  size += measureString(resolveElementTypeName(element.type));
  if (element.key != null) {
    size += measureString(element.key);
  }
  if (element.ref != null) {
    size += measureString(element.ref);
  }

  if (entries.length > 0) {
    size += SIZE.PROP * entries.length;
    for (const [key, value] of entries) {
      size += measureString(key);
      size += measureValue(value);
    }
  }

  if (children.length > 0) {
    size += SIZE.VALUE * children.length;
    for (const child of children) {
      size += measureValue(child);
    }
  }

  return size;
}

function measureValue(value) {
  switch (getValueType(value)) {
    case WasmValueType.Null:
    case WasmValueType.Undefined:
    case WasmValueType.Boolean:
    case WasmValueType.Number:
      return 0;
    case WasmValueType.String:
      return measureString(value);
    case WasmValueType.Element:
      return measureElement(value);
    case WasmValueType.Array:
      return measureArray(value);
    default:
      throw new TypeError('Unexpected value type encountered during measurement.');
  }
}

class WasmWriter {
  constructor(memory, malloc, totalSize) {
    if (!memory || typeof memory.buffer === 'undefined') {
      throw new TypeError('Expected a WebAssembly.Memory-like object.');
    }
    if (typeof malloc !== 'function') {
      throw new TypeError('malloc function is required');
    }
    this.totalSize = totalSize;
    this.baseAddress = malloc(totalSize);
    if (this.baseAddress === 0) {
      throw new Error('malloc returned 0 (out of memory)');
    }
    this.view = new DataView(memory.buffer, this.baseAddress, totalSize);
    this.bytes = new Uint8Array(memory.buffer, this.baseAddress, totalSize);
    this.offset = 0;
  }

  reserve(size) {
    const start = this.offset;
    this.offset += size;
    if (this.offset > this.totalSize) {
      throw new RangeError('WasmWriter reserve exceeded total buffer size');
    }
    return start;
  }

  writeString(value) {
    const text = String(value);
    const encoded = encoder.encode(text);
    const localOffset = this.reserve(encoded.length + 1);
    this.bytes.set(encoded, localOffset);
    this.bytes[localOffset + encoded.length] = 0;
    return this.baseAddress + localOffset;
  }

  zeroPayload(localOffset) {
    this.bytes.fill(0, localOffset, localOffset + SIZE.VALUE_PAYLOAD);
  }

  writeValueInto(localOffset, value) {
    const type = getValueType(value);
    this.view.setUint8(localOffset, type);
    const payloadOffset = localOffset + 1;
    this.zeroPayload(payloadOffset);

    switch (type) {
      case WasmValueType.Null:
      case WasmValueType.Undefined:
        return;
      case WasmValueType.Boolean:
        this.view.setUint8(payloadOffset, value ? 1 : 0);
        return;
      case WasmValueType.Number:
        this.view.setFloat64(payloadOffset, value, true);
        return;
      case WasmValueType.String: {
        const ptr = this.writeString(value);
        this.view.setUint32(payloadOffset, ptr, true);
        return;
      }
      case WasmValueType.Element: {
        const ptr = this.writeElement(value);
        this.view.setUint32(payloadOffset, ptr, true);
        return;
      }
      case WasmValueType.Array: {
        const ptr = this.writeArray(value);
        this.view.setUint32(payloadOffset, ptr, true);
        return;
      }
      default:
        throw new TypeError('Unsupported value type during serialization.');
    }
  }

  writeArray(values) {
    const arrayOffset = this.reserve(SIZE.ARRAY);
    const arrayPtr = this.baseAddress + arrayOffset;
    this.view.setUint32(arrayOffset, values.length, true);
    let itemsPtr = 0;
    if (values.length > 0) {
      const itemsOffset = this.reserve(SIZE.VALUE * values.length);
      itemsPtr = this.baseAddress + itemsOffset;
      for (let i = 0; i < values.length; ++i) {
        this.writeValueInto(itemsOffset + i * SIZE.VALUE, values[i]);
      }
    }
    this.view.setUint32(arrayOffset + 4, itemsPtr, true);
    return arrayPtr;
  }

  writeElement(element) {
    const offset = this.reserve(SIZE.ELEMENT);
    const elementPtr = this.baseAddress + offset;

    const typeNamePtr = this.writeString(resolveElementTypeName(element.type));
    const keyPtr = element.key == null ? 0 : this.writeString(element.key);
    const refPtr = element.ref == null ? 0 : this.writeString(element.ref);

    const props = element.props || {};
    const entries = Object.entries(props).filter(([key]) => key !== 'children');
    let propsPtr = 0;
    if (entries.length > 0) {
      propsPtr = this.reserve(SIZE.PROP * entries.length);
      for (let index = 0; index < entries.length; ++index) {
        const [key, value] = entries[index];
        const propOffset = propsPtr + index * SIZE.PROP;
        const keyPtrLocal = this.writeString(key);
        this.view.setUint32(propOffset, keyPtrLocal, true);
        this.writeValueInto(propOffset + 4, value);
      }
    }

    const children = [];
    collectChildren(props.children, children);
    let childrenPtr = 0;
    if (children.length > 0) {
      const childrenOffset = this.reserve(SIZE.VALUE * children.length);
      childrenPtr = this.baseAddress + childrenOffset;
      for (let index = 0; index < children.length; ++index) {
        this.writeValueInto(childrenOffset + index * SIZE.VALUE, children[index]);
      }
    }

    this.view.setUint32(offset + 0, typeNamePtr, true);
    this.view.setUint32(offset + 4, keyPtr, true);
    this.view.setUint32(offset + 8, refPtr, true);
    this.view.setUint32(offset + 12, entries.length, true);
    this.view.setUint32(offset + 16, propsPtr, true);
    this.view.setUint32(offset + 20, children.length, true);
    this.view.setUint32(offset + 24, childrenPtr, true);

    return elementPtr;
  }
}

function ensureReactElement(element) {
  if (!isReactElement(element)) {
    throw new TypeError('Expected a React element to serialize.');
  }
}

function writeElementTreeToWasmMemory(element, { memory, malloc }) {
  ensureReactElement(element);
  const totalSize = measureElement(element);
  const writer = new WasmWriter(memory, malloc, totalSize);
  const elementPtr = writer.writeElement(element);
  return {
    basePtr: writer.baseAddress,
    elementPtr,
    byteLength: totalSize,
  };
}

module.exports = {
  WasmValueType,
  SIZE,
  writeElementTreeToWasmMemory,
  measureElement,
  measureValue,
};
