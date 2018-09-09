'use strict';

let native = require('native');
let httpInternal = require('internal/http');

let tls = require('tls');

class Server extends tls.Server {
    keepAliveTimeout = 5000;
    timeout = 120000;
    maxHeadersCount = 2000;

    // event checkContinue
    // event checkExpectation
    // event upgrade
    // event connect

    constructor(options, acceptCallback) {
        if (!acceptCallback && typeof options === 'function') {
            acceptCallback = options;
            options = {};
        }
        super(options);

        this.on('request', acceptCallback);
        this._serverIncomingMessage = options && options.IncomingMessage ? options.IncomingMessage : httpInternal.IncomingMessage;
        this._serverServerResponse = options && options.ServerResponse ? options.ServerResponse : httpInternal.ServerResponse;

        this._httpServer = true;
        this.on('connection', (socket) => {
            httpInternal.handleServerConn(this, socket);
        });
    }
}

function createServer(options, acceptCallback) {
    return new Server(options, acceptCallback);
}

module.exports = {
    Server,
    createServer
}