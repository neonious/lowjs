'use strict';

// todo: keepAlive and keepAliveMsecs not used everywhere

let native = require('native');
let httpInternal = require('internal/http');

let net = require('net');
let url = require('url');
let stream = require('stream');

const {
    ERR_INVALID_ARG_TYPE,
    ERR_UNESCAPED_CHARACTERS
} = require('internal/errors').codes;

const INVALID_PATH_REGEX = /[^\u0021-\u00ff]/;

function validateHost(host, name) {
    if (host !== null && host !== undefined && typeof host !== 'string') {
        throw new ERR_INVALID_ARG_TYPE(`options.${name}`,
            ['string', 'undefined', 'null'],
            host);
    }
    return host;
}

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
}

class Agent {
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
        return net.createConnection(options, cb);
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

const discard2Headers = { 'age': 1, 'authorization': 1, 'content-length': 1, 'content-type': 1, 'etag': 1, 'expires': 1, 'from': 1, 'host': 1, 'if-modified-since': 1, 'if-unmodified-since': 1, 'last-modified': 1, 'location': 1, 'max-forwards': 1, 'proxy-authorization': 1, 'referer': 1, 'retry-after': 1, 'user-agent': 1 };

const tokenRegExp = /^[\^_`a-zA-Z\-0-9!#$%&'*+.|~]+$/;
/**
 * Verifies that the given val is a valid HTTP token
 * per the rules defined in RFC 7230
 * See https://tools.ietf.org/html/rfc7230#section-3.2.6
 */
function checkIsHttpToken(val) {
  return tokenRegExp.test(val);
}

class ClientRequest extends stream.Writable {
    constructor(options, cb) {
        super({
            write(chunk, encoding, callback) {
                if (!this.connection || this.connection.destroyed)
                    return;
                this.connection.bufferSize = this.connection.writableLength;
                this.connection._socketWriting = true;
                this.connection._updateRef();

                if (!this.headersSent)
                    this._sendHeaders();
                native.httpWrite(this.connection._socketFD, chunk, (err, bytesWrittenSocket) => {
                    this.connection._socketWriting = false;
                    this.connection._updateRef();
                    this.connection.bufferSize = this.connection.writableLength;
                    if (err)
                        this.destroy(err);
                    else {
                        this.connection.bytesWritten += bytesWrittenSocket;
                        callback(err);
                    }
                });
            },
            final(callback) {
                if (!this.connection) {
                    this._delayedEOFCallback = callback;
                    return;
                }
                if (this.connection.destroyed)
                    return;

                if (!this.headersSent)
                    this._sendHeaders();
                native.httpWrite(this.connection._socketFD, null, (err) => {
                    if (err)
                        this.destroy(err);

                    callback();
                });
            },
            destroy(err, callback) {
                if (this.connection)
                    this.connection.destroy(err, callback);
            }
        });
        this.cork();

        if (typeof options === 'string') {
            const urlStr = options;
/*            try {
                options = urlToOptions(new URL(urlStr));
            } catch (err) {
*/                options = url.parse(urlStr);
/*                if (!options.hostname) {
                    throw err;
                }
                if (!urlWarningEmitted && !process.noDeprecation) {
                    urlWarningEmitted = true;
                    process.emitWarning(
                        `The provided URL ${urlStr} is not a valid URL, and is supported ` +
                        'in the http module solely for compatibility.',
                        'DeprecationWarning', 'DEP0109');
                }
            }
        } else if (options && options[searchParamsSymbol] &&
            options[searchParamsSymbol][searchParamsSymbol]) {
            // url.URL instance
            options = urlToOptions(options);
        } else {
            options = util._extend({}, options);
        */        }

        var agent = options.agent;
        if (agent === false) {
            agent = new Agent();
        } else if (agent === null || agent === undefined) {
            if (typeof options.createConnection !== 'function') {
                agent = globalAgent;
            }
            // Explicitly pass through this statement as agent will not be used
            // when createConnection is provided.
        } else if (typeof agent.addRequest !== 'function') {
            throw new ERR_INVALID_ARG_TYPE('options.agent',
                ['Agent-like Object', 'undefined', 'false'],
                agent);
        }
        this.agent = agent;

        var protocol = options.protocol;
        var defaultPort = 80;

        var port = options.port = options.port || 80;
        var host = options.host = validateHost(options.hostname, 'hostname') ||
            validateHost(options.host, 'host') || 'localhost';

        var setHost = options.setHost === undefined || Boolean(options.setHost);

        this.timeout = options.timeout ? options.timeout : 120000;

        var method = options.method;
        var methodIsString = (typeof method === 'string');
        if (method !== null && method !== undefined && !methodIsString) {
            throw new ERR_INVALID_ARG_TYPE('method', 'string', method);
        }

        if (methodIsString && method) {
            if (!checkIsHttpToken(method)) {
                throw new ERR_INVALID_HTTP_TOKEN('Method', method);
            }
            method = this.method = method.toUpperCase();
        } else {
            method = this.method = 'GET';
        }

        if (options.path) {
            this.path = String(options.path);
            if (INVALID_PATH_REGEX.test(this.path))
                throw new ERR_UNESCAPED_CHARACTERS('Request path');
        } else
            this.path = '/';

        if (cb)
            this.once('response', cb);

        this.shouldKeepAlive = false;
        if (this.agent) {
            // If there is an agent we should default to Connection:keep-alive,
            // but only if the Agent will actually reuse the connection!
            // If it's not a keepAlive agent, and the maxSockets==Infinity, then
            // there's never a case where this socket will actually be reused
            if (!this.agent.keepAlive && !Number.isFinite(this.agent.maxSockets)) {
                this.shouldKeepAlive = false;
            } else {
                this.shouldKeepAlive = true;
            }
        }

        this._httpHeadersLower2Name = {};
        this._httpHeadersLowerCase = {};
        if (options.headers) {
            for (var name in options.headers) {
                let lower = name.toLowerCase();
                this._httpHeadersLower2Name[lower] = name;
                this._httpHeadersLowerCase[lower] = options.headers[name];
            }
        }

        if (host && !this._httpHeadersLowerCase['host'] && setHost) {
            var hostHeader = host;

            // For the Host header, ensure that IPv6 addresses are enclosed
            // in square brackets, as defined by URI formatting
            // https://tools.ietf.org/html/rfc3986#section-3.2.2
            var posColon = hostHeader.indexOf(':');
            if (posColon !== -1 &&
                hostHeader.indexOf(':', posColon + 1) !== -1 &&
                hostHeader.charCodeAt(0) !== 91/* '[' */) {
                hostHeader = `[${hostHeader}]`;
            }

            if (port && +port !== defaultPort) {
                hostHeader += ':' + port;
            }

            this._httpHeadersLower2Name['host'] = 'Host';
            this._httpHeadersLowerCase['host'] = hostHeader;
        }

        if (!this.shouldKeepAlive) {
            this._httpHeadersLower2Name['connection'] = 'Connection';
            this._httpHeadersLowerCase['connection'] = 'close';
        }
        if (options.auth && !this._httpHeadersLowerCase['authorization']) {
            this._httpHeadersLower2Name['authorization'] = 'Authorization';
            this._httpHeadersLowerCase['authorization'] = 'Basic ' +
                Buffer.from(options.auth).toString('base64');
        }

        this.on('agentRemove', () => {
            if (this.agent && this._agentOptions) {
                this.agent.removeSocket(this, this._agentOptions);
                this.agent = null;
            }
        });

        // Initiate connection
        if (this.agent) {
            this.agent.addRequest(this, options);
        } else {
            if (typeof options.createConnection === 'function') {
                let called = false;
                const newSocket = options.createConnection(options, (err, socket) => {
                    if (called)
                        return;
                    called = true;
                    if (err) {
                        this.destroy(err);
                        return;
                    }
                    this._onSocket(socket);
                });
                if (newSocket && !called) {
                    called = true;
                    this._onSocket(newSocket);
                }
            } else
                this._onSocket(net.createConnection(options));
        }
    }

    _onSocket(socket, options) {
        this._agentOptions = options;
        if (this.agent)
            socket.setTimeout(this.agent.timeout);

        if (!socket._socketFD) {
            socket.once('connect', () => {
                this._onSocket(socket, options);
            });

            return;
        }

        if (socket._socketReading)
            throw new Error("http socket already being read from");
        if (socket._socketHTTPWrapped)
            throw new Error("socket is already an http stream");

        socket._socketReading = true;
        socket._updateRef();
        socket._socketHTTPWrapped = true;

        this.connection = this.socket = socket;
        socket.setTimeout(this.timeout);

        native.httpGetRequest(socket._socketFD, (error, data, bytesRead) => {
            if (error) {
                this.destroy(error);
                return;
            }

            socket.bytesRead += bytesRead;

            socket._socketReading = false;
            socket._updateRef();

            let message = new httpInternal.IncomingMessage();
            message._httpMain = this;
            // this._httpMessage = message;

            message.connection = message.socket = socket;

            message.httpVersion = data[0];
            message.httpVersionMajor = data[0].charAt(0) | 0;
            message.httpVersionMinor = data[0].charAt(2) | 0;
            message.statusCode = data[1] | 0;
            message.statusMessage = data[2];

            message.rawHeaders = data.slice(3);
            message.headers = {};

            let upgrade = false;
            for (let i = 3; i + 1 < data.length; i += 2) {
                let key = data[i];
                let value = data[i + 1];

                key = key.toLowerCase();
                if (key == 'upgrade')
                    upgrade = true;
                if (message.headers[key]) {
                    if (key == 'set-cookie')
                        message.headers[key] = [value];
                    else if (!discard2headers[key])
                        message.headers[key] += ',' + value;
                } else if (key == 'set-cookie')
                    message.headers[key] = [value];
                else
                    message.headers[key] = value;
            }

            if (upgrade && this.listenerCount('upgrade')) {
                let buffer = native.httpDetach(socket._socketFD);
                socket._socketHTTPWrapped = false;
    
                if (this.emit('upgrade', message, socket, buffer))
                    return;
            }
            this.emit('response', message);
        });

        this.uncork();
        if (this._delayedEOFCallback) {
            if (!this.headersSent)
                this._sendHeaders();
            native.httpWrite(this.connection._socketFD, null, (err) => {
                this._delayedEOFCallback(err);
            });
        }
    }

    _sendHeaders() {
        this.headersSent = true;

        let len = -1;
        let contentLen = this._httpHeadersLowerCase['Content-Length'];
        let chunked = false;
        if (contentLen === undefined) {
            if (this.method === 'GET' ||
                this.method === 'HEAD' ||
                this.method === 'DELETE' ||
                this.method === 'OPTIONS' ||
                this.method === 'CONNECT')
                contentLen = 0;
        }
        if (contentLen !== undefined)
            len = contentLen | 0;  // to int
        else {
            this._httpHeadersLower2Name["transfer-encoding"] = "Transfer-Encoding";
            this._httpHeadersLowerCase["transfer-encoding"] = "chunked";
            chunked = true;
        }

        let headersAsTxt = this.method + ' ' + this.path + ' HTTP/1.1\r\n';
        for (let name in this._httpHeadersLowerCase)
            headersAsTxt += this._httpHeadersLower2Name[name] + ': ' + this._httpHeadersLowerCase[name] + '\r\n';
        headersAsTxt += '\r\n';

        native.httpWriteHead(this.connection._socketFD, headersAsTxt, len, chunked);
    }
}

function request(options, cb) {
    return new ClientRequest(options, cb);
}

function get(options, cb) {
    var req = request(options, cb);
    req.end();
    return req;
}

class Server extends net.Server {
    keepAliveTimeout = 5000;
    timeout = 120000;
    maxHeadersCount = 2000;

    // event checkContinue
    // event checkExpectation
    // event connect

    constructor(options, acceptCallback) {
        if (!acceptCallback && typeof options === 'function') {
            acceptCallback = options;
            options = {};
        }

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
    Agent,
    globalAgent,
    METHODS: httpInternal.METHODS,
    STATUS_CODES: httpInternal.STATUS_CODES,
    IncomingMessage: httpInternal.IncomingMessage,
    ServerResponse: httpInternal.ServerResponse,
    Server,
    createServer,
    get,
    request
}