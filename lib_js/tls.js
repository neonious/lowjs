'use strict';

let native = require('native');
let net = require('net');

class Server extends net.Server {
    constructor(options, acceptCallback) {
        if (!acceptCallback && typeof options === 'function') {
            acceptCallback = options;
            options = {};
        }
        if (!options)
            options = {};

        options.secureContext = native.createTLSContext(options);
        super(options);
    }
}

function createServer(options, acceptCallback) {
    return new Server(options, acceptCallback);
}

module.exports = {
    Server,
    createSecureContext: native.createTLSContext,
    createServer
}