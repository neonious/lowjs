'use strict';

let native = require('native');
let httpInternal = require('internal/http');

let tls = require('tls');

/*
function calculateServerName(options, req) {
    let servername = options.host;
    const hostHeader = req.getHeader('host');
    if (hostHeader) {
        // abc => abc
        // abc:123 => abc
        // [::1] => ::1
        // [::1]:123 => ::1
        if (hostHeader.startsWith('[')) {
            const index = hostHeader.indexOf(']');
            if (index === -1) {
                // Leading '[', but no ']'. Need to do something...
                servername = hostHeader;
            } else {
                servername = hostHeader.substr(1, index - 1);
            }
        } else {
            servername = hostHeader.split(':', 1)[0];
        }
    }
    return servername;
}*/

class Agent {
    defaultPort = 443;

    requests = {};
    sockets = {};
    freeSockets = {};

    constructor(options) {
        this.options = Object.assign({}, options);
        // don't confuse net and make it think that we're connecting to a pipe
        this.options.path = null;

        this.keepAlive = options && options.keepAlive !== undefined ? options.keepAlive : false;
        this.keepAliveMsecs = options && options.keepAliveMsecs !== undefined ? options.keepAliveMsecs : 1000;
        this.maxSockets = options && options.maxSockets !== undefined ? options.maxSockets : Infinity;
        this.maxFreeSockets = options && options.maxFreeSockets !== undefined ? options.maxFreeSockets : 256;
        this.timeout = options && options.timeout !== undefined ? options.timeout : 120000;
    }

    removeSocket(socket, options, name) {
        if (!name)
            name = this.getName(options);
        var sets = [this.sockets, this.freeSockets];
        for (var sk = 0; sk < sets.length; sk++) {
            var sockets = sets[sk];
            if (sockets[name]) {
                var index = sockets[name].indexOf(socket);
                if (index !== -1) {
                    sockets[name].splice(index, 1);
                    // Don't leak
                    if (sockets[name].length === 0)
                        delete sockets[name];
                }
            }
        }
        socket.destroy();

        if (this.requests[name] && this.requests[name].length) {
            var req = this.requests[name][0];
            // If we are under maxSockets create a new one.
            let socket = this.createConnection(options);
            req._onSocket(socket, options);
            this.sockets[name].push(socket);
        }
    }

    freeSocket(socket, options) {
        socket.setTimeout(0);

        var name = this.getName(options);
        if (this.requests[name] && this.requests[name].length) {
            this.requests[name].shift()._onSocket(socket, options);
            if (this.requests[name].length === 0) {
                // don't leak
                delete this.requests[name];
            }
        } else {
            if (this.keepAlive) {
                var freeSockets = this.freeSockets[name];
                var freeLen = freeSockets ? freeSockets.length : 0;
                var count = freeLen;
                if (this.sockets[name])
                    count += this.sockets[name].length;

                this.removeSocket(socket, options, name);
                if (count > this.maxSockets || freeLen >= this.maxFreeSockets) {
                    socket.destroy();
                } else if (this.keepSocketAlive(socket)) {
                    freeSockets = freeSockets || [];
                    this.freeSockets[name] = freeSockets;
                    freeSockets.push(socket);
                } else {
                    // Implementation doesn't want to keep socket alive
                    socket.destroy();
                }
            } else {
                // Implementation doesn't want to keep socket alive
                socket.destroy();
            }
        }
    }

    keepSocketAlive(socket) {
        socket.setKeepAlive(true, this.keepAliveMsecs);
        socket.unref();
        return true;
    }
    reuseSocket(socket) {
        socket.ref();
    }

    createConnection(options, cb) {
        return tls.connect(options, cb);
    }

    destroy() {
        var sets = [this.freeSockets, this.sockets];
        for (var s = 0; s < sets.length; s++) {
            var set = sets[s];
            var keys = Object.keys(set);
            for (var v = 0; v < keys.length; v++) {
                var setName = set[keys[v]];
                for (var n = 0; n < setName.length; n++) {
                    setName[n].destroy();
                }
            }
        }
    }

    getName(options) {
        var name = options.host || 'localhost';

        name += ':';
        if (options.port)
            name += options.port;

        name += ':';
        if (options.localAddress)
            name += options.localAddress;

        // Pacify parallel/test-http-agent-getname by only appending
        // the ':' when options.family is set.
        if (options.family === 4 || options.family === 6)
            name += `:${options.family}`;

        if (options.socketPath)
            name += `:${options.socketPath}`;

        return name;
    }

    addRequest(request, options) {
        options = Object.assign({}, options);
        Object.assign(options, this.options);
        if (options.socketPath)
            options.path = options.socketPath;
        //        if (!options.servername)        // todo: is this even used?
        //            options.servername = calculateServerName(options, request);

        var name = this.getName(options);
        if (!this.sockets[name]) {
            this.sockets[name] = [];
        }
        var freeLen = this.freeSockets[name] ? this.freeSockets[name].length : 0;
        var sockLen = freeLen + this.sockets[name].length;

        if (freeLen) {
            // we have a free socket, so use that.
            var socket = this.freeSockets[name].shift();
            if (!this.freeSockets[name].length)
                delete this.freeSockets[name];

            this.reuseSocket(socket, request);
            request._onSocket(socket, options);
            this.sockets[name].push(socket);
        } else if (sockLen < this.maxSockets) {
            // If we are under maxSockets create a new one.
            let socket = this.createConnection(options);
            request._onSocket(socket, options);
            this.sockets[name].push(socket);
        } else {
            // We are over limit so we'll add it to the queue.
            if (!this.requests[name]) {
                this.requests[name] = [];
            }
            this.requests[name].push(request);
        }
    }
}
let globalAgent = new Agent();

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

function request(options, cb) {
    if (options.agent === false)
        options.agent = new Agent(options);
    else if (options.agent === null || options.agent === undefined) {
        if (typeof options.createConnection !== 'function') {
            options.agent = globalAgent;
        }
    }
    return new httpInternal.ClientRequest(options, cb);
}

function get(options, cb) {
    var req = request(options, cb);
    req.end();
    return req;
}

module.exports = {
    Server,
    createServer,
    Agent,
    request,
    get,
    globalAgent
}