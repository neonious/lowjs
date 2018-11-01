'use strict';

// TIMEOUTS??

let native = require('native');
let stream = require('stream');

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

class IncomingMessage extends stream.Readable {
    // event "aborted"
    aborted = false;

    constructor() {
        super({
            read(size) {
                if (this.connection.destroyed)
                    return;
                if (this._isServer && this._httpMain.headersSent) {
                    this.push(null);
                    return;
                }

                let buf = new Buffer(size);
                this.connection._socketReading = true;
                this.connection._updateRef();

                native.httpRead(this.connection._socketFD, buf, (err, bytesRead, bytesReadSocket, trailers, reuse) => {
                    this.connection._socketReading = false;
                    this.connection._updateRef();
                    if (err) {
                        this.destroy(err);
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

                        if (!this._isServer) {
                            if (this._httpMain.agent) {
                                let socket = this.socket;
                                this.connection = this.socket = null;
                                this._httpMain.connection = this._httpMain.socket = null;
                                if (reuse) {
                                    socket._socketHTTPWrapped = false;
                                    this._httpMain.agent.freeSocket(socket, this._httpMain._agentOptions);
                                } else
                                    this._httpMain.agent.removeSocket(socket, this._httpMain._agentOptions);
                            }
                            this.destroy();
                        }
                        return;
                    }
                    this.connection.bytesRead += bytesReadSocket;
                    this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                });
            },
            destroy(err, callback) {
                this._httpMain.destroy();
                callback(err);
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
                if (this.connection.destroyed)
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
                        callback();
                    }
                });
            },
            final(callback) {
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
                this.connection.destroy(err, callback);
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
        let contentLen = this._httpHeadersLowerCase['Content-Length'];
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
        this._httpHeadersLower2Name["connection"] = "Connection";
        this._httpHeadersLowerCase["connection"] = "keep-alive";
        if (contentLen !== undefined)
            len = contentLen | 0;  // to int
        else if (this._httpMessage.httpVersion == '1.1') {
            this._httpHeadersLower2Name["transfer-encoding"] = "Transfer-Encoding";
            this._httpHeadersLowerCase["transfer-encoding"] = "chunked";
            chunked = true;
        } else
            this._httpHeadersLowerCase["connection"] = "close";

        let headersAsTxt = 'HTTP/1.1 ' + this.statusCode + ' ' + this.statusMessage + '\r\n';
        for (let name in this._httpHeadersLowerCase)
            headersAsTxt += this._httpHeadersLower2Name[name] + ': ' + this._httpHeadersLowerCase[name] + '\r\n';
        headersAsTxt += '\r\n';

        native.httpWriteHead(this.connection._socketFD, headersAsTxt, len, chunked);
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
    getHeaderNames() { }
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

const discard2Headers = { 'age': 1, 'authorization': 1, 'content-length': 1, 'content-type': 1, 'etag': 1, 'expires': 1, 'from': 1, 'host': 1, 'if-modified-since': 1, 'if-unmodified-since': 1, 'last-modified': 1, 'location': 1, 'max-forwards': 1, 'proxy-authorization': 1, 'referer': 1, 'retry-after': 1, 'user-agent': 1 };

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

module.exports = {
    METHODS,
    STATUS_CODES,
    IncomingMessage,
    ServerResponse,
    handleServerConn
}