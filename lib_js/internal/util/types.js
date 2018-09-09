'use strict';

const ReflectApply = Reflect.apply;

// This function is borrowed from the function with the same name on V8 Extras'
// `utils` object. V8 implements Reflect.apply very efficiently in conjunction
// with the spread syntax, such that no additional special case is needed for
// function calls w/o arguments.
// Refs: https://github.com/v8/v8/blob/d6ead37d265d7215cf9c5f768f279e21bd170212/src/js/prologue.js#L152-L156
function uncurryThis(func) {
    return (thisArg, ...args) => ReflectApply(func, thisArg, args);
}

// Cached to make sure no userland code can tamper with it.
const isArrayBufferView = ArrayBuffer.isView;

function isTypedArray(value) {
    return false;
}

function isUint8Array(value) {
    return value instanceof Uint8Array;
}

function isUint8ClampedArray(value) {
    return value instanceof Uint8ClampedArray;
}

function isUint16Array(value) {
    return value instanceof Uint16Array;
}

function isUint32Array(value) {
    return value instanceof Uint32Array;
}

function isInt8Array(value) {
    return value instanceof Int8Array;
}

function isInt16Array(value) {
    return value instanceof Int16Array;
}

function isInt32Array(value) {
    return value instanceof Int32Array;
}

function isFloat32Array(value) {
    return value instanceof Float32Array;
}

function isFloat64Array(value) {
    return value instanceof Float64Array;
}

function isBigInt64Array(value) {
    return value instanceof BigInt64Array;
}

function isBigUint64Array(value) {
    return value instanceof BigUint64Array;
}

function isAnyArrayBuffer(value) { return false; }
function isArgumentsObject(value) { return false; }
function isDataView(value) { return value instanceof DataView; }
function isExternal(value) { return false; }
function isMap(value) { return value instanceof Map; }
function isMapIterator(value) { return false; }
function isPromise(value) { return value instanceof Promise; }
function isSet(value) { return value instanceof Set; }
function isSetIterator(value) { return false; }
function isWeakMap(value) { return value instanceof WeakMap; }
function isWeakSet(value) { return value instanceof WeakSet; }
function isRegExp(value) { return value instanceof RegExp; }
function isDate(value) { return value instanceof Date; }
function isTypedArray(value) { return false; }
function isModuleNamespaceObject(value) { return false; }
// TODO: check list of util/types

module.exports = {
    isArrayBufferView,
    isTypedArray,
    isUint8Array,
    isUint8ClampedArray,
    isUint16Array,
    isUint32Array,
    isInt8Array,
    isInt16Array,
    isInt32Array,
    isFloat32Array,
    isFloat64Array,
    isBigInt64Array,
    isBigUint64Array,
    isAnyArrayBuffer,
    isArgumentsObject,
    isDataView,
    isExternal,
    isMap,
    isMapIterator,
    isPromise,
    isSet,
    isSetIterator,
    isWeakMap,
    isWeakSet,
    isRegExp,
    isDate,
    isTypedArray,
    isModuleNamespaceObject
};
