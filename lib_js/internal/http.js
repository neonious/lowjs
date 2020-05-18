'use strict';

// TIMEOUTS??

let native = require('native');
let stream = require('stream');
let url = require('url');

const {
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

const METHODS = ['ACL',
    'BIND',
    'CHECKOUT',
    'CONNECT',
    'COPY',
    'DELETE',
    'GET',
    'HEAD',
    'LINK',
    'LOCK',
    'M-SEARCH',
    'MERGE',
    'MKACTIVITY',
    'MKCALENDAR',
    'MKCOL',
    'MOVE',
    'NOTIFY',
    'OPTIONS',
    'PATCH',
    'POST',
    'PROPFIND',
    'PROPPATCH',
    'PURGE',
    'PUT',
    'REBIND',
    'REPORT',
    'SEARCH',
    'SUBSCRIBE',
    'TRACE',
    'UNBIND',
    'UNLINK',
    'UNLOCK',
    'UNSUBSCRIBE'];
const STATUS_CODES = {
    100: 'Continue',
    101: 'Switching Protocols',
    102: 'Processing',                 // RFC 2518, obsoleted by RFC 4918
    103: 'Early Hints',
    200: 'OK',
    201: 'Created',
    202: 'Accepted',
    203: 'Non-Authoritative Information',
    204: 'No Content',
    205: 'Reset Content',
    206: 'Partial Content',
    207: 'Multi-Status',               // RFC 4918
    208: 'Already Reported',
    226: 'IM Used',
    300: 'Multiple Choices',           // RFC 7231
    301: 'Moved Permanently',
    302: 'Found',
    303: 'See Other',
    304: 'Not Modified',
    305: 'Use Proxy',
    307: 'Temporary Redirect',
    308: 'Permanent Redirect',         // RFC 7238
    400: 'Bad Request',
    401: 'Unauthorized',
    402: 'Payment Required',
    403: 'Forbidden',
    404: 'Not Found',
    405: 'Method Not Allowed',
    406: 'Not Acceptable',
    407: 'Proxy Authentication Required',
    408: 'Request Timeout',
    409: 'Conflict',
    410: 'Gone',
    411: 'Length Required',
    412: 'Precondition Failed',
    413: 'Payload Too Large',
    414: 'URI Too Long',
    415: 'Unsupported Media Type',
    416: 'Range Not Satisfiable',
    417: 'Expectation Failed',
    418: 'I\'m a Teapot',              // RFC 7168
    421: 'Misdirected Request',
    422: 'Unprocessable Entity',       // RFC 4918
    423: 'Locked',                     // RFC 4918
    424: 'Failed Dependency',          // RFC 4918
    425: 'Unordered Collection',       // RFC 4918
    426: 'Upgrade Required',           // RFC 2817
    428: 'Precondition Required',      // RFC 6585
    429: 'Too Many Requests',          // RFC 6585
    431: 'Request Header Fields Too Large', // RFC 6585
    451: 'Unavailable For Legal Reasons',
    500: 'Internal Server Error',
    501: 'Not Implemented',
    502: 'Bad Gateway',
    503: 'Service Unavailable',
    504: 'Gateway Timeout',
    505: 'HTTP Version Not Supported',
    506: 'Variant Also Negotiates',    // RFC 2295
    507: 'Insufficient Storage',       // RFC 4918
    508: 'Loop Detected',
    509: 'Bandwidth Limit Exceeded',
    510: 'Not Extended',               // RFC 2774
    511: 'Network Authentication Required' // RFC 6585
};

const discard2headers = { 'age': 1, 'authorization': 1, 'content-length': 1, 'content-type': 1, 'etag': 1, 'expires': 1, 'from': 1, 'host': 1, 'if-modified-since': 1, 'if-unmodified-since': 1, 'last-modified': 1, 'location': 1, 'max-forwards': 1, 'proxy-authorization': 1, 'referer': 1, 'retry-after': 1, 'user-agent': 1 };

class IncomingMessage extends stream.Readable {
    // event "aborted"
    aborted = false;

    constructor() {
        super({
            read(size) {
                if (!this.connection || this.connection.destroyed)
                    return;

                let buf = new Buffer(size);
                this.connection._socketReading = true;
                this.connection._updateRef();

                native.httpRead(this.connection._socketFD, buf, (err, bytesRead, bytesReadSocket, trailers, reuse) => {
                    if (!this.connection)
                        return;

                    this.connection._socketReading = false;
                    this.connection._updateRef();
                    if (err) {
                        this.connection.emit('error', err);
                        return;
                    }

                    if (this.connection._timeout)
                        this.connection._timeout.refresh();
                    if (bytesRead == 0) {
                        this.rawTrailers = trailers;
                        this.trailers = {};

                        for (let i = 0; i + 1 < trailers.length; i += 2) {
                            let key = trailers[i];
                            let value = trailers[i + 1];

                            key = key.toLowerCase();
                            if (this.trailers[key]) {
                                if (key == 'set-cookie')
                                    this.trailers[key] = [value];
                                else if (!discard2headers[key])
                                    this.trailers[key] += ',' + value;
                            } else if (key == 'set-cookie')
                                this.trailers[key] = [value];
                            else
                                this.trailers[key] = value;
                        }

                        this.push(null);
                        if (this._isServer) {
                            if(!reuse)
                                this._httpMain.destroy();
                        } else {
                            if(this._httpMain.agent) {
                                let socket = this.socket;
                                this.connection = this.socket = null;
                                this._httpMain.connection = this._httpMain.socket = null;

                                delete socket._httpSetup;
                                if (reuse) {
                                    socket._socketHTTPWrapped = false;
                                    this._httpMain.agent.freeSocket(socket, this._httpMain._agentOptions);
                                } else {
                                    this._httpMain.agent.removeSocket(socket, this._httpMain._agentOptions);
                                    socket.destroy();
                                }
                            }
                            this._httpMain.destroy();
                        }
                } else {
                        this.connection.bytesRead += bytesReadSocket;
                        this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                    }
                });
            },
            destroy(err, callback) {
                this._httpMain.destroy(err);
                callback();
            }
        });
    }

    setTimeout(msecs, callback) { }
}

class ServerResponse extends stream.Writable {
    finished = false;
    statusCode = "200";
    sendDate = true;
    headersSent = false;

    _httpHeadersLowerCase = {};
    _httpHeadersLower2Name = {};

    constructor() {
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
                    if (!this.connection)
                        return;

                    this.connection._socketWriting = false;
                    this.connection._updateRef();
                    this.connection.bufferSize = this.connection.writableLength;
                    if (err)
                        this.connection.emit('error', err);
                    else {
                        this.connection.bytesWritten += bytesWrittenSocket;
                        callback();
                    }
                });
            },
            final(callback) {
                if (!this.connection || this.connection.destroyed)
                    return;

                if (!this.headersSent) {
                    this._httpHeadersLower2Name["content-length"] = "Content-Length";
                    this._httpHeadersLowerCase["content-length"] = "0";
                    this._sendHeaders();
                }
                this._httpMessage.resume();

                native.httpWrite(this.connection._socketFD, null, (err) => {
                    if (err)
                        this.connection.emit('error', err);

                    callback();
                });
            },
            destroy(err, callback) {
                if (this.connection) {
                    this.connection.destroy(err);
                    callback();
                } else
                    callback(err);
            }
        });
    }

    _sendHeaders() {
        this.headersSent = true;

        if (!this.statusMessage)
            this.statusMessage = STATUS_CODES[this.statusCode];
        if (!this.statusMessage)
            this.statusMessage = "unknown";

        let len = -1;
        let contentLen = this._httpHeadersLowerCase['content-length'];
        let chunked = false;

        /*
                // Duktape seems to return 2018-08-09 21:27:43.806Z
                // instead of the string which ends with GMT
                // But in any case, we have to think about this because with ESP32
                // the date is not always set correctly...
                if (this.sendDate) {
                    this._httpHeadersLower2Name["date"] = "Date";
                    this._httpHeadersLowerCase["date"] = new Date().toUTCString();
                }
        */
        if(!this._httpHeadersLower2Name["connection"]) {
            // Close by default, as embedded system cannot handle many sockets
            this._httpHeadersLower2Name["connection"] = "Connection";
            this._httpHeadersLowerCase["connection"] = "close";
        }
        if (contentLen !== undefined)
            len = contentLen | 0;  // to int
        else if (this._httpMessage.httpVersion == '1.1') {
            this._httpHeadersLower2Name["transfer-encoding"] = "Transfer-Encoding";
            this._httpHeadersLowerCase["transfer-encoding"] = "chunked";
            chunked = true;
        }

        let headersAsTxt = 'HTTP/1.1 ' + this.statusCode + ' ' + this.statusMessage + '\r\n';
        for (let name in this._httpHeadersLowerCase)
            headersAsTxt += this._httpHeadersLower2Name[name] + ': ' + this._httpHeadersLowerCase[name] + '\r\n';
        headersAsTxt += '\r\n';

        native.httpWriteHead(this.connection._socketFD, headersAsTxt, len, chunked);
    }

    _implicitHeader() {
        return this.writeHead(this.statusCode);
    }

    setHeader(name, value) {
        if (this.headersSent)
            return;

        let lower = name.toLowerCase();
        this._httpHeadersLower2Name[lower] = name;
        this._httpHeadersLowerCase[lower] = value;
    }

    hasHeader(name) {
        return this._httpHeadersLowerCase[name.toLowerCase()] !== undefined;
    }

    removeHeader(name) {
        if (this.headersSent)
            return;

        let lower = name.toLowerCase();
        delete this._httpHeadersLower2Name[lower];
        delete this._httpHeadersLowerCase[lower];
    };

    getHeader(name) {
        return this._httpHeadersLowerCase[name.toLowerCase()];
    }

    getHeaders() {
        return this._httpHeadersLowerCase;
    }

    addTrailers(headers) { }

    getHeaderNames() {
        return Object.keys(this._httpHeadersLowerCase);
    }

    setTimeout(msecs, callback) { }
    writeContinue() { }

    writeHead(statusCode, statusMessage, headers) {
        if (this.headersSent)
            return;

        if (!headers && typeof statusMessage !== 'string') {
            headers = statusMessage;
            statusMessage = undefined;
        }
        if (statusCode)
            this.statusCode = statusCode;
        if (statusMessage)
            this.statusMessage = statusMessage;

        if (headers) {
            for (let name in headers)
                this.setHeader(name, headers[name]);
        }
    }

    writeProcessing() { }
}

function handleServerConn(server, socket) {
    if (socket._socketReading)
        throw new Error("http socket already being read from");
    if (socket._socketHTTPWrapped)
        throw new Error("socket is already an http stream");

    socket._socketReading = true;
    socket._updateRef();
    socket._socketHTTPWrapped = true;

    let inError = false;
    function handleError(err) {
        socket._socketHTTPWrapped = false;
        if (inError) {
            socket.destroy(err);
            return;
        }
        inError = true;

        if (!server.emit('clientError', err, socket))
            socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
    }
    socket.on('error', handleError);

    native.httpGetRequest(socket._socketFD, (error, data, bytesRead) => {
        if (error) {
            socket.emit('error', error);
            return;
        }

        socket.bytesRead += bytesRead;
        socket._socketReading = false;
        socket._updateRef();

        let message = new server._serverIncomingMessage();
        let response = new server._serverServerResponse();
        message._isServer = true;
        message._httpMain = response;
        response._httpMessage = message;

        message.connection = message.socket =
            response.connection = response.socket = socket;

        message.method = data[0];
        message.url = data[1];
        message.httpVersion = data[2];
        message.httpVersionMajor = data[2].charAt(0) | 0;
        message.httpVersionMinor = data[2].charAt(2) | 0;

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

        message.on('error', handleError);
        response.on('error', handleError);

        if (upgrade && server.listenerCount('upgrade')) {
            let buffer = native.httpDetach(socket._socketFD);
            socket._socketHTTPWrapped = false;

            if (server.emit('upgrade', message, socket, buffer))
                return;
        }
        server.emit('request', message, response);
    });
}

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
                        this.connection.emit('error', err);
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
                        this.connection.emit('error', err);

                    callback();
                });
            },
            destroy(err, callback) {
                if (this.connection) {
                    this.connection.destroy(err);
                    callback();
                } else
                    callback(err);
            }
        });
        this._no_uncork_on_end = true;
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
        if (agent && typeof agent.addRequest !== 'function') {
            throw new ERR_INVALID_ARG_TYPE('options.agent',
                ['Agent-like Object', 'undefined', 'false'],
                agent);
        }
        this.agent = agent;

        var protocol = options.protocol;

        var port = options.port = options.port || (agent && agent.defaultPort);
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

            if (port && (!agent || +port !== agent.defaultPort)) {
                hostHeader += ':' + port;
            }

            this._httpHeadersLower2Name['host'] = 'Host';
            this._httpHeadersLowerCase['host'] = hostHeader;
        }

        if (!this.shouldKeepAlive && !this._httpHeadersLower2Name['connection']) {
            this._httpHeadersLower2Name['connection'] = 'Connection';
            this._httpHeadersLowerCase['connection'] = 'close';
        }
        if (options.auth && !this._httpHeadersLowerCase['authorization']) {
            this._httpHeadersLower2Name['authorization'] = 'Authorization';
            this._httpHeadersLowerCase['authorization'] = 'Basic ' +
                Buffer.from(options.auth).toString('base64');
        }

        // Initiate connection
        if (this.agent)
            this.agent.addRequest(this, options);
        else {
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
        }
    }

    _onSocket(socket, options) {
        this._agentOptions = options;

        if(socket._httpSetup != this) {
            socket._httpSetup = this;
            socket.once('close', () => {
                if(socket._httpSetup != this)
                    return;

                if (this.agent && this._agentOptions) {
                    this.agent.removeSocket(socket, this._agentOptions);
                    this.agent = null;
                }
                this.destroy();
            });
            socket.once('agentRemove', () => {
                if(socket._httpSetup != this)
                    return;

                if (this.agent && this._agentOptions) {
                    this.agent.removeSocket(socket, this._agentOptions);
                    this.agent = null;
                }
            });
            socket.once('error', (err) => {
                if(socket._httpSetup != this)
                    return;

                this.emit('error', err);
            });
        }

        if (socket.connecting) {
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
                socket.emit('error', error);
                return;
            }

            socket.bytesRead += bytesRead;

            socket._socketReading = false;
            socket._updateRef();

            let message = new IncomingMessage();
            message._httpMain = this;
            this._httpMessage = message;

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
        let contentLen = this._httpHeadersLowerCase['content-length'];
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

    abort() {
        this.aborted = true;
        if(this._httpMessage)
            this._httpMessage.aborted = true;

        this.emit('abort');
        if(this._httpMessage)
            this._httpMessage.emit('aborted');
        if(this._httpMessage)
            this._httpMessage.destroy();
        else
            this.destroy();
    }
}

module.exports = {
    METHODS,
    STATUS_CODES,
    IncomingMessage,
    ServerResponse,
    handleServerConn,
    ClientRequest
}