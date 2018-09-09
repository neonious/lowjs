// init.js
// runs before main module, exports = global scope

let native = require('native');
let events = require('events');

/*
Error.captureStackTrace = function (obj, constructor) {
    // Breaks test/assert_throws_stack.js
    // TODO: only take after constructor
    var fakeObj = {
        getFileName: () => { return ''; },
        getLineNumber: () => { return 0; },
        getColumnNumber: () => { return 0; },
        isEval: () => { return false; },
        getEvalOrigin: () => { return ''; },
        getFunctionName: () => { return ''; }
    };
    obj.stack = [fakeObj, fakeObj, fakeObj];//new Error().stack; -- this should be an array with getFileName and so on.. it is not! So setting it to empty
};
*/

class Timeout {
    constructor(func, delay, oneshot) {
        this._func = func;
        this._delay = delay;
        this._oneshot = oneshot;
        this._id = native.setChore(func, delay, oneshot);
    }
    ref() {
        native.choreRef(this._id, true);
    }
    unref() {
        native.choreRef(this._id, false);
    }
    refresh() {
        native.clearChore(this._id);
        this._id = native.setChore(this._func, this._delay, this._oneshot);
    }
}
exports.setTimeout = function (func, delay) {
    var cb_func;
    var bind_args;

    if (delay === undefined)
        delay = 0;
    else if (typeof delay !== 'number') {
        throw new TypeError('delay is not a number');
    }

    if (typeof func === 'string') {
        // Legacy case: callback is a string.
        cb_func = eval.bind(this, func);
    } else if (typeof func !== 'function') {
        throw new TypeError('callback is not a function/string');
    } else if (arguments.length > 2) {
        // Special case: callback arguments are provided.
        bind_args = Array.prototype.slice.call(arguments, 2);  // [ arg1, arg2, ... ]
        bind_args.unshift(this);  // [ global(this), arg1, arg2, ... ]
        cb_func = func.bind.apply(func, bind_args);
    } else {
        // Normal case: callback given as a function without arguments.
        cb_func = func;
    }

    return new Timeout(cb_func, delay, true);
}
exports.setInterval = function (func, delay) {
    var cb_func;
    var bind_args;

    if (delay === undefined)
        delay = 0;
    else if (typeof delay !== 'number') {
        throw new TypeError('delay is not a number');
    }

    if (typeof func === 'string') {
        // Legacy case: callback is a string.
        cb_func = eval.bind(this, func);
    } else if (typeof func !== 'function') {
        throw new TypeError('callback is not a function/string');
    } else if (arguments.length > 2) {
        // Special case: callback arguments are provided.
        bind_args = Array.prototype.slice.call(arguments, 2);  // [ arg1, arg2, ... ]
        bind_args.unshift(this);  // [ global(this), arg1, arg2, ... ]
        cb_func = func.bind.apply(func, bind_args);
    } else {
        // Normal case: callback given as a function without arguments.
        cb_func = func;
    }
    return new Timeout(cb_func, delay, false);
}

class Immediate {
    ref() {
        native.choreRef(this._id, true);
    }
    unref() {
        native.choreRef(this._id, false);
    }
}
exports.setImmediate = function (func) {
    var cb_func;
    var bind_args;

    if (typeof func === 'string') {
        // Legacy case: callback is a string.
        cb_func = eval.bind(this, func);
    } else if (typeof func !== 'function') {
        throw new TypeError('callback is not a function/string');
    } else if (arguments.length > 2) {
        // Special case: callback arguments are provided.
        bind_args = Array.prototype.slice.call(arguments, 1);  // [ arg1, arg2, ... ]
        bind_args.unshift(this);  // [ global(this), arg1, arg2, ... ]
        cb_func = func.bind.apply(func, bind_args);
    } else {
        // Normal case: callback given as a function without arguments.
        cb_func = func;
    }

    let obj = new Immediate();
    obj._id = native.setChore(cb_func, 0, true);
    return obj;
}

function clear(obj) {
    if (obj)
        native.clearChore(obj._id);
}
exports.clearTimeout = clear;
exports.clearInterval = clear;
exports.clearImmediate = clear;

// https://raw.githubusercontent.com/lahmatiy/es6-promise-polyfill/master/promise.min.js
// + function gets exports directly (look at end)
(function (t) {
    function z() { for (var a = 0; a < g.length; a++)g[a][0](g[a][1]); g = []; m = !1 } function n(a, b) { g.push([a, b]); m || (m = !0, A(z, 0)) } function B(a, b) { function c(a) { p(b, a) } function h(a) { k(b, a) } try { a(c, h) } catch (d) { h(d) } } function u(a) { var b = a.owner, c = b.state_, b = b.data_, h = a[c]; a = a.then; if ("function" === typeof h) { c = l; try { b = h(b) } catch (d) { k(a, d) } } v(a, b) || (c === l && p(a, b), c === q && k(a, b)) } function v(a, b) {
        var c; try {
            if (a === b) throw new TypeError("A promises callback cannot return that same promise."); if (b && ("function" ===
                typeof b || "object" === typeof b)) { var h = b.then; if ("function" === typeof h) return h.call(b, function (d) { c || (c = !0, b !== d ? p(a, d) : w(a, d)) }, function (b) { c || (c = !0, k(a, b)) }), !0 }
        } catch (d) { return c || k(a, d), !0 } return !1
    } function p(a, b) { a !== b && v(a, b) || w(a, b) } function w(a, b) { a.state_ === r && (a.state_ = x, a.data_ = b, n(C, a)) } function k(a, b) { a.state_ === r && (a.state_ = x, a.data_ = b, n(D, a)) } function y(a) { var b = a.then_; a.then_ = void 0; for (a = 0; a < b.length; a++)u(b[a]) } function C(a) { a.state_ = l; y(a) } function D(a) { a.state_ = q; y(a) } function e(a) {
        if ("function" !==
            typeof a) throw new TypeError("Promise constructor takes a function argument"); if (!1 === this instanceof e) throw new TypeError("Failed to construct 'Promise': Please use the 'new' operator, this object constructor cannot be called as a function."); this.then_ = []; B(a, this)
    } var f = t.Promise, s = f && "resolve" in f && "reject" in f && "all" in f && "race" in f && function () { var a; new f(function (b) { a = b }); return "function" === typeof a }(); "undefined" !== typeof exports && exports ? (exports.Promise = s ? f : e, exports.Polyfill = e) : "function" ==
        typeof define && define.amd ? define(function () { return s ? f : e }) : s || (t.Promise = e); var r = "pending", x = "sealed", l = "fulfilled", q = "rejected", E = function () { }, A = "undefined" !== typeof setImmediate ? setImmediate : setTimeout, g = [], m; e.prototype = { constructor: e, state_: r, then_: null, data_: void 0, then: function (a, b) { var c = { owner: this, then: new this.constructor(E), fulfilled: a, rejected: b }; this.state_ === l || this.state_ === q ? n(u, c) : this.then_.push(c); return c.then }, "catch": function (a) { return this.then(null, a) } }; e.all = function (a) {
            if ("[object Array]" !==
                Object.prototype.toString.call(a)) throw new TypeError("You must pass an array to Promise.all()."); return new this(function (b, c) { function h(a) { e++; return function (c) { d[a] = c; --e || b(d) } } for (var d = [], e = 0, f = 0, g; f < a.length; f++)(g = a[f]) && "function" === typeof g.then ? g.then(h(f), c) : d[f] = g; e || b(d) })
        }; e.race = function (a) {
            if ("[object Array]" !== Object.prototype.toString.call(a)) throw new TypeError("You must pass an array to Promise.race()."); return new this(function (b, c) {
                for (var e = 0, d; e < a.length; e++)(d = a[e]) && "function" ===
                    typeof d.then ? d.then(b, c) : b(d)
            })
        }; e.resolve = function (a) { return a && "object" === typeof a && a.constructor === this ? a : new this(function (b) { b(a) }) }; e.reject = function (a) { return new this(function (b, c) { c(a) }) }
})(exports);

// https://www.npmjs.com/package/es6-collections/master/es6-collections.js
// + function gets exports directly (look at end)
(function (e) {
    function f(a, c) { function b(a) { if (!this || this.constructor !== b) return new b(a); this._keys = []; this._values = []; this._itp = []; this.objectOnly = c; a && v.call(this, a) } c || w(a, "size", { get: x }); a.constructor = b; b.prototype = a; return b } function v(a) { this.add ? a.forEach(this.add, this) : a.forEach(function (a) { this.set(a[0], a[1]) }, this) } function d(a) { this.has(a) && (this._keys.splice(b, 1), this._values.splice(b, 1), this._itp.forEach(function (a) { b < a[0] && a[0]-- })); return -1 < b } function m(a) {
        return this.has(a) ? this._values[b] :
            void 0
    } function n(a, c) { if (this.objectOnly && c !== Object(c)) throw new TypeError("Invalid value used as weak collection key"); if (c != c || 0 === c) for (b = a.length; b-- && !y(a[b], c);); else b = a.indexOf(c); return -1 < b } function p(a) { return n.call(this, this._values, a) } function q(a) { return n.call(this, this._keys, a) } function r(a, c) { this.has(a) ? this._values[b] = c : this._values[this._keys.push(a) - 1] = c; return this } function t(a) { this.has(a) || this._values.push(a); return this } function h() {
        (this._keys || 0).length = this._values.length =
            0
    } function z() { return k(this._itp, this._keys) } function l() { return k(this._itp, this._values) } function A() { return k(this._itp, this._keys, this._values) } function B() { return k(this._itp, this._values, this._values) } function k(a, c, b) { var g = [0], e = !1; a.push(g); return { next: function () { var f, d = g[0]; !e && d < c.length ? (f = b ? [c[d], b[d]] : c[d], g[0]++) : (e = !0, a.splice(a.indexOf(g), 1)); return { done: e, value: f } } } } function x() { return this._values.length } function u(a, c) {
        for (var b = this.entries(); ;) {
            var d = b.next(); if (d.done) break;
            a.call(c, d.value[1], d.value[0], this)
        }
    } var b, w = Object.defineProperty, y = function (a, b) { return isNaN(a) ? isNaN(b) : a === b }; "undefined" == typeof WeakMap && (e.WeakMap = f({ "delete": d, clear: h, get: m, has: q, set: r }, !0)); "undefined" != typeof Map && "function" === typeof (new Map).values && (new Map).values().next || (e.Map = f({ "delete": d, has: q, get: m, set: r, keys: z, values: l, entries: A, forEach: u, clear: h })); "undefined" != typeof Set && "function" === typeof (new Set).values && (new Set).values().next || (e.Set = f({
        has: p, add: t, "delete": d, clear: h,
        keys: l, values: l, entries: B, forEach: u
    })); "undefined" == typeof WeakSet && (e.WeakSet = f({ "delete": d, add: t, clear: h, has: p }, !0))
})(exports);

exports.process = new events.EventEmitter();
native.processInfo(process, (signal) => {
    return process.emit(signal);
});
process.nextTick = setImmediate;

let ttyinfo = native.ttyInfo();
if (ttyinfo) {
    // tty version
    let tty = require('tty');
    process.stdin = new tty.ReadStream(0);
    process.stdout = new tty.WriteStream(1);
    process.stderr = new tty.WriteStream(2);

    process.stdout.rows = ttyinfo.rows;
    process.stdout.columns = ttyinfo.columns;
    process.stderr.rows = ttyinfo.rows;
    process.stderr.columns = ttyinfo.columns;

    process.on('SIGWINCH', () => {
        let ttyinfo = native.ttyInfo();
        if (ttyinfo) {
            if (process.stdout) {
                process.stdout.rows = ttyinfo.rows;
                process.stdout.columns = ttyinfo.columns;
            }
            if (process.stderr) {
                process.stderr.rows = ttyinfo.rows;
                process.stderr.columns = ttyinfo.columns;
            }
            if (process.stdout)
                process.stdout.emit('resize');
            if (process.stderr)
                process.stderr.emit('resize');
        }
    });
} else {
    // without tty
    let net = require('net');

    process.stdin = new net.Socket({
        fd: 0,
        readable: true,
        writable: false
    });
    process.stdout = new net.Socket({
        fd: 1,
        readable: false,
        writable: true
    });
    process.stderr = new net.Socket({
        fd: 2,
        readable: false,
        writable: true
    });
}
process.stdin.on('pause', () => {
    process.stdin.unref();
});
process.stdin.on('resume', () => {
    process.stdin.ref();
});

exports.console = require('console');

Buffer.from = (...args) => { return new Buffer(...args); }
Buffer.alloc = (...args) => { return new Buffer(...args); }