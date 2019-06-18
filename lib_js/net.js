let native = require('native');
let events = require('events');
let stream = require('stream');
let dns = require('dns');

class Socket extends stream.Duplex {
    connecting = false;
    _ref = true;

    bytesRead = 0;
    bytesWritten = 0;
    bufferSize = 0;

    constructor(options) {
        if (options && options.secureContext)
            this._secureContext = options.secureContext;

        this._socketFD = options ? options.fd : undefined;
        super({
            allowHalfOpen: options && options.allowHalfOpen,
            read(size) {
                if (options && !this.readable)
                    throw new Error("reading from stream not allowed");
                if (this._socketHTTPWrapped)
                    throw new Error("socket is an http stream, writing not allowed");

                if (this._socketFD === undefined) {
                    this._waitConnect = size;
                    this._updateRef();
                    return;
                }

                let buf = new Buffer(size);
                this._socketReading = true;
                this._updateRef();
                native.read(this._socketFD, buf, 0, size, null, (err, bytesRead) => {
                    this._socketReading = false;
                    this._updateRef();
                    if (err) {
                        this.destroy(err);
                        return;
                    }

                    if (this._timeout)
                        this._timeout.refresh();
                    if (bytesRead == 0) {
                        if (this._writableEOF)
                            this.destroy();
                        else
                            this.push(null);
                        return;
                    }
                    this.bytesRead += bytesRead;
                    this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                });
            },
            write(chunk, encoding, callback) {
                if (options && !this.writable)
                    throw new Error("writing to stream not allowed");
                if (this._socketHTTPWrapped)
                    throw new Error("socket is an http stream, reading not allowed");

                this.bufferSize = this.writableLength;
                this._socketWriting = true;
                this._updateRef();
                native.write(this._socketFD, chunk, 0, chunk.length, null, (err, bytesWritten) => {
                    this._socketWriting = false;
                    this._updateRef();
                    this.bufferSize = this.writableLength;
                    if (err)
                        this.destroy(err);
                    else {
                        this.bytesWritten += bytesWritten;
                        if (chunk.length != bytesWritten) {
                            this._write(chunk.slice(bytesWritten), encoding, callback);
                            return;
                        }
                        callback();
                    }
                });
            },
            final(callback) {
                if (this._readableEOF) {
                    this.destroy();
                    callback();
                } else
                    native.shutdown(this._socketFD, callback);
            },
            destroy(err, callback) {
                if (this._timeout) {
                    clearTimeout(this._timeout);
                    delete this._timeout;
                }

                native.close(this._socketFD, (err2) => {
                    if (err || err2) {
                        callback(err || err2);
                        return;
                    }

                    this.unref();
                    if (this._socketServer && this._socketServer.connections) {
                        if (--this._socketServer.connections == 0 && !this._socketServer.listening)
                            this._socketServer.emit('close');
                    }
                    callback();
                });
            }
        });

        this.readable = this._socketFD === undefined ? true : (options ? !!options.readable : false);
        this.writable = this._socketFD === undefined ? true : (options ? !!options.writable : false);

        if (this._socketFD === undefined)
            this.cork();

        this._updateRef();
        this.on('close', () => {
            this._updateRef();
        });
    }

    connect(...args) {
        if (this.destroyed)
            throw new Error('Socket already closed.');
        if (this._socketFD !== undefined || this.connecting)
            throw new Error('Socket already connected/connecting.');
        this.connecting = true;

        let normalized;
        // If passed an array, it's treated as an array of arguments that have
        // already been normalized (so we don't normalize more than once). This has
        // been solved before in https://github.com/nodejs/node/pull/12342, but was
        // reverted as it had unintended side effects.
        if (Array.isArray(args[0]) && args[0][normalizedArgsSymbol])
            normalized = args[0];
        else
            normalized = normalizeArgs(args);
        let options = normalized[0];
        let cb = normalized[1];

        let family = options.port === undefined ? 0 : native.isIP(options.host);
        if (options.port === undefined || family)
            this._connect(options, family, options.port === undefined ? options.path : options.host, cb);
        else
            dns.lookup(options.host, (err, host, family) => {
                this.emit('lookup', err, options.host, family, host);
                if (err) {
                    this.connecting = false;
                    this._updateRef();

                    this.emit('error', err);
                    return;
                }
                if (this.destroyed) {
                    this.connecting = false;
                    this._updateRef();
                    return;
                }

                this._connect(options, family, host, cb);
            });

        return this;
    }
    _connect(options, family, address, callback) {
        this._updateRef();
        native.connect(family, address, options.port | 0, this._secureContext, (err, fd, family, localHost, localPort, remoteHost, remotePort) => {
            if (err) {
                this.connecting = false;
                this._updateRef();

                this.emit('error', err);
                return;
            }
            if (this.destroyed) {
                native.close(fd, (err) => {
                    this.connecting = false;
                    this._updateRef();
                    if (err)
                        this.emit('error', err);
                });
                return;
            }

            this._socketFD = fd;
            native.setsockopt(this._socketFD, this._sockoptKeepaliveEnabled, this._sockoptKeepaliveSecs, this._sockoptNoDelay);
            if (family == 4)
                this.remoteFamily = 'IPv4';
            else if (family == 6)
                this.remoteFamily = 'IPv6';
            this.localAddress = localHost;
            this.localPort = localPort;
            this.remoteAddress = remoteHost;
            this.remotePort = remotePort;

            this.connecting = false;
            this.uncork();
            this._updateRef();

            if (this._waitConnect !== undefined) {
                let size = this._waitConnect;
                delete this._waitConnect;
                this._socketReading = true;

                let buf = new Buffer(size);
                native.read(this._socketFD, buf, 0, size, null, (err, bytesRead) => {
                    this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                });
            }

            this.emit('connect');
            this.emit('ready');
            if (this._timeout)
                this._timeout.refresh();

            if (callback)
                callback(null);
        });
    }

    address() {
        return { port: this.localPort, family: this.remoteFamily, address: this.localAddress };
    }

    setKeepAlive(setting, msecs) {
        this._sockoptKeepaliveEnabled = !!setting;
        if (msecs)
            this._sockoptKeepaliveSecs = ~~(msecs / 1000);
        if (!this.connecting && !this.destroyed && !this.connecting)
            native.setsockopt(this._socketFD, this._sockoptKeepaliveEnabled, this._sockoptKeepaliveSecs);
        return this;
    }

    setNoDelay(enable) {
        // backwards compatibility: assume true when `enable` is omitted
        this._sockoptNoDelay = enable === undefined ? true : !!enable;
        if (!this.connecting && !this.destroyed && !this.connecting)
            native.setsockopt(this._socketFD, undefined, undefined, this._sockoptNoDelay);
        return this;
    }

    setTimeout(timeout, callback) {
        if (this.destroyed)
            throw new Error('Socket already closed.');
        if (callback)
            this.once('timeout', callback);

        if (this._timeout)
            clearTimeout(this._timeout);
        if (timeout) {
            this._timeout = setTimeout(() => {
                this._timeout = null;
                this.emit('timeout');
            }, timeout + 110);
        }
        return this;
    }

    ref() {
        this._ref = true;
        this._updateRef();
        return this;
    }
    unref() {
        this._ref = false;
        this._updateRef();
        return this;
    }
    _updateRef() {
        if (this._ref && !this.destroyed && (this.connecting || this._waitConnect || this._socketReading || this._socketWriting)) {
            if (!this._refSet) {
                native.runRef(1);
                this._refSet = true;
            }
        } else {
            if (this._refSet) {
                native.runRef(-1);
                this._refSet = false;
            }
        }
    }
}

class Server extends events.EventEmitter {
    _address = null;
    listening = false;
    destroyed = false;

    connections = 0;

    constructor(options, connectionListener) {
        super();

        if (typeof options === 'function') {
            connectionListener = options;
            options = {};
            this.on('connection', connectionListener);
        } else if (options == null || typeof options === 'object') {
            options = options || {};
            if (typeof connectionListener === 'function')
                this.on('connection', connectionListener);
        } else {
            throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
        }

        if (options && options.secureContext)
            this._secureContext = options.secureContext;
    }

    address() {
        return this._address;
    }

    close(callback) {
        if (this.destroyed)
            return this;
        if (this.listening)
            native.close(this._serverFD, (err) => {
                if (err) {
                    this.emit('error', err);
                }
                if (callback)
                    callback(err);
            });

        this.unref();
        this.destroyed = true;
        this.listening = false;

        if (!this.connections) {
            this.emit('close');
            if (callback)
                callback(null);
        }

        return this;
    }

    getConnections(callback) {
        callback(null, this.connections);
    }

    listen(...args) {
        if (this.destroyed)
            throw new Error('Server already closed.');
        if (this.listening || this._tryListen)
            throw new Error('Server already listening.');
        this._tryListen = true;

        let normalized = normalizeArgs(args);
        let options = normalized[0];
        let cb = normalized[1];

        if (!options.host)
            options.host = '::';
        let family = options.port === undefined ? 0 : native.isIP(options.host);
        if (options.port === undefined || family)
            this._listen(options, family, options.port === undefined ? options.path : options.host, cb);
        else
            dns.lookup(options.host, (err, host, family) => {
                this.emit('lookup', err, options.host, family, host);
                if (err) {
                    this._tryListen = false;
                    this.emit('error', err);
                    return;
                }

                this._listen(options, family, host, cb);
            });

        return this;
    }
    _listen(options, family, address, callback) {
        this.ref();
        native.listen(family, address, options.port | 0, !!this._httpServer, this._secureContext, (err, fd, port) => {
            this._tryListen = false;
            if (err) {
                this.unref();
                this.emit('error', err);
                return;
            }
            if (this.destroyed) {
                native.close(fd, (err) => {
                    if (err)
                        this.emit('error', err);
                });
                return;
            }
            this.listening = true;

            this._serverFD = fd;
            if (family)
                this._address = { family: 'IPv' + family, address, port };
            else
                this._address = address;

            setImmediate(() => {    // required for test ...test/parallel/test-http-1.0.js, and maybe others
                this.emit('listening');
                if (callback)
                    callback(null);
            });
        }, (err, fd, family, localHost, localPort, remoteHost, remotePort) => {
            if (err) {
                // May happen if TLS connection goes wrong
                // We do not even have a real socket, yet, so keep this silent
                return;
            }
            if (this.destroyed || (this.maxConnections !== undefined && this.connections >= this.maxConnections)) {
                native.close(fd, (err) => {
                    if (err)
                        this.emit('error', err);
                });
                return;
            }
            this.connections++;

            let socket = new Socket();

            socket._socketServer = this;
            socket._socketFD = fd;
            if (family == 4)
                socket.remoteFamily = 'IPv4';
            else if (family == 6)
                socket.remoteFamily = 'IPv6';
            socket.localAddress = localHost;
            socket.localPort = localPort;
            socket.remoteAddress = remoteHost;
            socket.remotePort = remotePort;
            socket.uncork();
            socket.ref();

            this.emit('connection', socket);
            socket.emit('ready');
        });
    }

    ref() {
        if (this.destroyed)
            throw new Error('Server already closed.');
        if (!this._ref)
            native.runRef(1);
        this._ref = true;
        return this;
    }
    unref() {
        if (this._ref)
            native.runRef(-1);
        this._ref = false;
        return this;
    }
}

function createConnection(...args) {
    let normalized = normalizeArgs(args);
    let options = normalized[0];

    let socket = new Socket(options);
    let cb = normalized[1];
    if(cb)
        socket.once('connect', cb);
    if (options.timeout)
        socket.setTimeout(options.timeout);
    return Socket.prototype.connect.call(socket, options);
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

function createServer(options, connectionListener) {
    return new Server(options, connectionListener);
}

module.exports = {
    Socket,
    Server,
    Stream: Socket,     // legacy
    connect: createConnection,
    createConnection,
    createServer,
    isIP: native.isIP,
    isIPv4: (input) => { return native.isIP(input) == 4; },
    isIPv6: (input) => { return native.isIP(input) == 6; }
}