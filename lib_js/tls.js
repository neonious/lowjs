'use strict';

let native = require('native');
let net = require('net');
let socket = require('stream');

class Server extends net.Server {
    constructor(options, acceptCallback) {
        if (!acceptCallback && typeof options === 'function') {
            acceptCallback = options;
            options = {};
        }
        if (!options)
            options = {};

        options.secureContext = native.createTLSContext(options, true);
        super(options);
    }
}

class TLSSocket extends net.Socket {
    constructor(options, secureConnectCallback) {
        if (!secureConnectCallback && typeof options === 'function') {
            secureConnectCallback = options;
            options = {};
        }
        if (!options)
            options = {};

        options.secureContext = native.createTLSContext(options, false);
        super(options);
    }
}

function createServer(options, acceptCallback) {
    return new Server(options, acceptCallback);
}

function connect(...args) {
    let normalized = normalizeArgs(args);
    let options = normalized[0];

    let socket = new TLSSocket(options);
    let cb = normalized[1];
    if(cb)
        socket.once('connect', cb);
    if (options.timeout)
        socket.setTimeout(options.timeout);
    return TLSSocket.prototype.connect.call(socket, options);
}
// Returns an array [options, cb], where options is an object,
// cb is either a function or null.
// Used to normalize arguments of Socket.prototype.connect() and
// Server.prototype.listen(). Possible combinations of parameters:
//   (options[...][, cb])
//   (path[...][, cb])
//   ([port][, host][...][, cb])
// For Socket.prototype.connect(), the [...] part is ignored
// For Server.prototype.listen(), the [...] part is [, backlog]
// but will not be handled here (handled in listen())
let normalizedArgsSymbol = Symbol();
function isPipeName(s) {
    return typeof s === 'string' && toNumber(s) === false;
}
function normalizeArgs(args) {
    let arr;

    if (args.length === 0) {
        arr = [{}, null];
        arr[normalizedArgsSymbol] = true;
        return arr;
    }

    const arg0 = args[0];
    let options = {};
    if (typeof arg0 === 'object' && arg0 !== null) {
        // (options[...][, cb])
        options = arg0;
    } else if (isPipeName(arg0)) {
        // (path[...][, cb])
        options.path = arg0;
    } else {
        // ([port][, host][...][, cb])
        options.port = arg0;
        if (args.length > 1 && typeof args[1] === 'string') {
            options.host = args[1];
        }
    }

    let cb = args[args.length - 1];
    if (typeof cb !== 'function')
        arr = [options, null];
    else
        arr = [options, cb];

    arr[normalizedArgsSymbol] = true;
    return arr;
}

module.exports = {
    Server,
    createSecureContext: native.createTLSContext,
    createServer,
    connect
}