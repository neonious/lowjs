'use strict';

// TODO: stream.req ?

let EventEmitter = require('events').EventEmitter;

const {
    ERR_STREAM_PREMATURE_CLOSE,
    ERR_INVALID_CALLBACK,
    ERR_MISSING_ARGS
} = require('internal/errors').codes;

class Readable extends EventEmitter {
    readable = true;
    destroyed = false;

    readableFlowing = null;
    readableHighWaterMark = 0;
    readableLength = 0;

    _readableObjectMode = false;
    _readableEmitClose = true;
    _readableEncoding = null;
    _readableBuf = [];
    _readableReading = false;
    _readableEOF = false;
    _readablePipes = {};

    _readableState = {
        finished: false,
        errorEmitted: false
    };

    constructor(options) {
        super();

        if (options && options.emitClose !== undefined)
            this._readableEmitClose = options.emitClose;
        if (options && options.encoding !== undefined)
            this._readableEncoding = options.encoding;
        if (options && options.objectMode !== undefined)
            this._readableObjectMode = options.objectMode;
        this.readableHighWaterMark = options && options.highWaterMark !== undefined ? options.highWaterMark : (this._readableObjectMode ? 16 : 16384);
        if (options && options.read)
            this._read = options.read.bind(this);
        if (options && options.destroy)
            this._destroy = options.destroy.bind(this);

        if (!this._read) {
            this._read = (size) => { };
        }
        if (!this._destroy) {
            this._destroy = (err, cb) => { cb(err); };
        }

        this.on('newListener', (event) => {
            if (event == 'data' && this.readableFlowing === null)
                this.resume();
        });
    }

    setEncoding(encoding) {
        this._readableEncoding = encoding;
        return this;
    }

    destroy(err) {
        if (this.destroyed) // before, we were emitting the error here even if destroyed,
            // but this may result into infinite recursion
            return this;
        this.destroyed = true;

        this._readableEOF = this._readableState.finished = true;

        this._destroy(err, (err) => {
            if (err) {
                this._readableState.errorEmitted = true;
                this.emit('error', err);
            }
            if (this._readableEmitClose)
                this.emit('close');
        });
        return this;
    }

    isPaused() {
        return this.readableFlowing === true;
    }

    pause() {
        this.emit('pause');
        this.readableFlowing = false;
        if (!this._readableEOF && !this._readableReading && this.readableLength < this.readableHighWaterMark) {
            if (!this._readableCalling)
                this._readableCalling = setImmediate(() => { this._reconsiderRead() });
        }
        return this;
    }

    _reconsiderRead() {
        this._readableCalling = null;
        if (!this._readableEOF && !this._readableReading && this.readableLength < this.readableHighWaterMark) {
            this._readableReading = true;
            this._read(1024);
        }
    }

    resume() {
        this.emit('resume');
        this.readableFlowing = true;
        while (this.readableFlowing === true && this._readableBuf.length)
            this.emit('data', this.read());
        if (this.readableFlowing === true && this._readableEOF && !this._readableState.finished) {
            this._readableState.finished = true;
            this.emit('end');
        }

        if (!this._readableCalling)
            this._readableCalling = setImmediate(() => { this._reconsiderRead() });
        return this;
    }

    read(len) {
        if (this._readableObjectMode)
            len = 1;

        if (len === undefined || this._readableEOF)
            len = this.readableLength;
        else if (len > this.readableLength)
            return null;

        let res = null;

        this.readableLength -= len;
        if (this._readableObjectMode)
            res = this._readableBuf.shift();
        else
            while (len) {
                let chunk = this._readableBuf.shift();
                if (chunk.length > len) {
                    this._readableBuf.unshift(chunk.slice(len));
                    chunk = chunk.slice(0, len);
                    len = 0;
                } else
                    len -= chunk.length;

                if (typeof chunk === 'string')
                    res = res ? res + chunk : chunk;
                else
                    res = res ? Buffer.concat([res, chunk]) : chunk;
            }

        if (!this._readableEOF && !this._readableReading && this.readableLength < this.readableHighWaterMark) {
            if (!this._readableCalling)
                this._readableCalling = setImmediate(() => { this._reconsiderRead() });
        }
        return res;
    }

    unshift(chunk) {
        if (this._readableState.finished)
            return;
        if (this._readableEncoding)
            chunk = chunk.toString(this._readableEncoding);

        this._readableBuf.unshift(chunk);
        this.readableLength += this._readableObjectMode ? 1 : chunk.length;
    }

    push(chunk) {
        if (this._readableEOF) {
            this._readableReading = false;
            return false;
        }
        this.emit('push', chunk);

        if (chunk === null)
            this._readableEOF = true;
        if (this._readableObjectMode || chunk === null || chunk.length) {
            if (this.readableFlowing === true) {
                if (chunk === null) {
                    this._readableState.finished = true;
                    this.emit('end');
                } else {
                    if (this._readableEncoding)
                        chunk = chunk.toString(this._readableEncoding);
                    this.emit('data', chunk);
                }
            } else {
                if (chunk !== null) {
                    if (this._readableEncoding)
                        chunk = chunk.toString(this._readableEncoding);

                    this._readableBuf.push(chunk);
                    this.readableLength += this._readableObjectMode ? 1 : chunk.length;
                }
                this.emit('readable');
            }
        }

        this._readableReading = false;
        if (!this._readableEOF && this.readableLength < this.readableHighWaterMark) {
            if (!this._readableCalling)
                this._readableCalling = setImmediate(() => { this._reconsiderRead() });
            return true;
        } else {
            return false;
        }
    }

    pipe(destination, options) {
        let end = options && options.end !== undefined ? options.end : true;

        let pipe = this._readablePipes[destination] = [
            () => {
                this.resume();
            },
            (chunk) => {
                let drain = destination.write(chunk, this._readableEncoding);
                if (!drain && this.readableFlowing)
                    this.pause();
                else if (drain && !this.readableFlowing)
                    this.resume();
            },
            end ? (chunk) => {
                destination.end();
            } : null,
            () => {
                this.unpipe(destination);
            }
        ];

        destination.on('drain', pipe[0]);
        this.on('data', pipe[1]);
        if (end)
            this.on('end', pipe[2]);
        this._readableState.errorEmitted = true;
        this.on('error', pipe[3]);
        destination._writableState.errorEmitted = true;
        destination.on('error', pipe[3]);

        destination.emit('pipe', this);

        if (Object.keys(this._readablePipes).length == 1)
            this.resume();

        return destination;
    }

    unpipe(destination) {
        let pipe = this._readablePipes[destination];
        if (!pipe)
            return;

        destination.emit('unpipe', this);

        destination.removeListener('drain', pipe[0]);
        this.removeListener('data', pipe[1]);
        if (pipe[2])
            this.removeListener('end', pipe[2]);
        this.removeListener('error', pipe[3]);
        destination.removeListener('error', pipe[3]);

        delete this._readablePipes[destination];
        if (Object.keys(this._readablePipes).length == 0)
            this.pause();
    }

    wrap(stream) {
        var paused = false;

        stream.on('end', () => {
            this.push(null);
        });

        stream.on('data', (chunk) => {
            var ret = this.push(chunk);
            if (!ret) {
                paused = true;
                stream.pause();
            }
        });

        // proxy all the other methods.
        // important when wrapping filters and duplexes.
        for (var i in stream) {
            if (this[i] === undefined && typeof stream[i] === 'function') {
                this[i] = function (method) {
                    return function () {
                        return stream[method].apply(stream, arguments);
                    };
                }(i);
            }
        }

        // proxy certain important events.
        const kProxyEvents = ['error', 'close', 'destroy', 'pause', 'resume'];
        for (var n = 0; n < kProxyEvents.length; n++) {
            stream.on(kProxyEvents[n], this.emit.bind(this, kProxyEvents[n]));
        }

        // when we try to consume some more bytes, simply unpause the
        // underlying stream.
        this._read = (n) => {
            if (paused) {
                paused = false;
                stream.resume();
            }
        };

        return this;
    }
}

class Writable extends EventEmitter {
    writable = true;
    destroyed = false;

    writableLength = 0;
    writableHighWaterMark = 0;

    _writableObjectMode = false;
    _writableDecodeStrings = true;
    _writableEmitClose = true;
    _writableEmittedHighMark = false;

    _writableEOF = false;
    _writableBuf = [];
    _writableCorkCount = 0;

    _writableState = {
        finished: false,
        errorEmitted: false
    };

    constructor(options) {
        super();

        if (options && options.objectMode !== undefined)
            this._writableObjectMode = options.objectMode;
        this.writableHighWaterMark = options && options.highWaterMark !== undefined ? options.highWaterMark : (this._writableObjectMode ? 16 : 16384);
        if (options && options.decodeStrings !== undefined)
            this._writableDecodeStrings = options.decodeStrings;
        if (options && options.emitClose !== undefined)
            this._writableEmitClose = options.emitClose;

        if (options && options.write)
            this._write = options.write.bind(this);

        if (!this._write) {
            this._write = (chunk, encoding, callback) => { callback(); };
        }
        if (!this._final) {
            this._final = (callback) => { callback(); };
        }
        if (!this._destroy) {
            this._destroy = (err, cb) => { cb(err); };
        }

        if (options && options.final)
            this._final = options.final.bind(this);
        if (options && options.destroy)
            this._destroy = options.destroy.bind(this);
    }

    destroy(err) {
        if (this.destroyed) // before, we were emitting the error here even if destroyed,
            // but this may result into infinite recursion
            return this;
        this.destroyed = true;

        this._writableBuf = [];
        this._writableEOF = this._writableState.finished = true;

        this._destroy(err, (err) => {
            if (err) {
                this._writableState.errorEmitted = true;
                this.emit('error', err);
            }
            if (this._writableEmitClose)
                this.emit('close');
        });
        return this;
    }

    setDefaultEncoding(encoding) {
        this._writableEncoding = encoding;
        return this;
    }

    _writableNext() {
        if (this._writableDestroy)
            return;
        if (this._writableEmittedHighMark && this.writableLength < this.writableHighWaterMark && !this._writableEOF) {
            this._writableEmittedHighMark = false;
            this.emit('drain');
        }

        if (this._writableCorkCount == 0) {
            if (this._writableBuf.length) {
                this._writableWriting = true;
                if (this._writev) {
                    let newBuf = [];
                    for (let i = 0; i < this._writableBuf.length; i++) {
                        let entry = this._writableBuf[i];
                        newBuf.push({ chunk: entry[0], encoding: entry[1] });
                        this.writableLength -= this._writableObjectMode ? 1 : entry[0].length;
                    }
                    let saveBuf = this._writableBuf;
                    this._writev(newBuf, (err) => { if (err) { this._writableState.errorEmitted = true; this.emit('error', err); } for (let i = 0; i < saveBuf.length; i++) for (let j = 2; j < saveBuf[i].length; j++) saveBuf[i][j](); this._writableNext(); });
                }
                let entry = this._writableBuf.shift();
                this.writableLength -= this._writableObjectMode ? 1 : entry[0].length;
                this._write(entry[0], entry[1], (err) => { if (err) this.emit('error', err); for (let j = 2; j < entry.length; j++) entry[j](); this._writableNext(); });
            } else {
                if (this._writableEOF && !this._writableState.finished) {
                    this._writableState.finished = true;

                    if (this._final)
                        this._final((err) => { if (err) { this._writableState.errorEmitted = true; this.emit('error', err); } this.emit('finish'); });
                    else
                        this.emit('finish');
                }
                this._writableWriting = false;
            }
        } else
            this._writableWriting = false;
    }

    cork() {
        this._writableCorkCount++;
        return this;
    }

    uncork() {
        this._writableCorkCount--;
        if (this._writableCorkCount < 0)
            this._writableCorkCount = 0;
        if (this._writableCorkCount == 0 && !this._writableWriting)
            this._writableNext();
        return this;
    }

    write(chunk, encoding, callback) {
        if (this._writableEOF)
            return false;

        if (!callback && encoding && typeof encoding === 'function') {
            callback = encoding;
            encoding = null;
        }
        if (!encoding)
            encoding = this._writableEncoding;
        if (!callback)
            callback = () => { };
        if (this._writableDecodeStrings && typeof chunk === 'string')
            chunk = new Buffer(chunk, encoding);

        if (this._writableCorkCount == 0 && !this._writableWriting) {
            this._writableWriting = true;
            this._write(chunk, encoding, (err) => { if (err) { this._writableState.errorEmitted = true; this.emit('error', err); } process.nextTick(callback); this._writableNext(); });
        } else {
            if (!this._writableObjectMode && this._writableBuf.length) {
                let last = this._writableBuf[this._writableBuf.length - 1];
                if (last[0].length + chunk.length < 1024 && last[1] == encoding) {
                    if (typeof last[0] == 'string' && typeof chunk == 'string')
                        last[0] += chunk;
                    else
                        last[0] = Buffer.concat([last[0], chunk]);
                    last.push(callback);
                }
                else
                    this._writableBuf.push([chunk, encoding, callback]);
            } else
                this._writableBuf.push([chunk, encoding, callback]);
            this.writableLength += this._writableObjectMode ? 1 : chunk.length;
        }
        if (this.writableLength < this.writableHighWaterMark)
            return true;
        else {
            this._writableEmittedHighMark = true;
            return false;
        }
    }

    end(chunk, encoding, callback) {
        if (this._writableEOF)
            return this;

        if (!callback) {
            if (!encoding) {
                if (typeof chunk === 'function') {
                    callback = chunk;
                    chunk = null;
                }
            } else {
                if (typeof encoding === 'function') {
                    callback = encoding;
                    encoding = null;
                }
            }
        }

        if (chunk) {
            this.write(chunk, encoding, () => {
                this.end(null, null, callback);
            });
            return this;
        }
        if (callback)
            this.once('finish', callback);

        this._writableEOF = true;
        this._writableCorkCount = 0;
        if (!this._writableWriting)
            this._writableNext();

        return this;
    }
}

class Duplex extends Readable {
    constructor(options) {
        super(options);
        Writable.bind(this)(options);

        if (options && !options.allowHalfOpen)
            this.on('end', this.end);

        if (options && options.objectMode === undefined && options.readableObjectMode !== undefined)
            this._readableObjectMode = options.readableObjectMode;
        if (options && options.objectMode === undefined && options.writableObjectMode !== undefined)
            this._writableObjectMode = options.writableObjectMode;
        if (!options || options.highWaterMark === undefined) {
            this.readableHighWaterMark = options && options.readableHighWaterMark !== undefined ? options.readableHighWaterMark : (this._readableObjectMode ? 16 : 16384);
            this.writableHighWaterMark = options && options.writableHighWaterMark !== undefined ? options.writableHighWaterMark : (this._writableObjectMode ? 16 : 16384);
        }
    }
}

Object.defineProperty(Writable, Symbol.hasInstance, {
    value: function (object) {
        return Function[Symbol.hasInstance].call(this, object) || object instanceof Duplex;
    }
});

Object.assign(Duplex.prototype, Writable.prototype);
Duplex.prototype.destroy = function (err) {
    if (this.destroyed) // before, we were emitting the error here even if destroyed,
        // but this may result into infinite recursion
        return this;
    this.destroyed = true;

    this._readableEOF = this._readableState.finished = true;
    this._writableBuf = [];
    this._writableEOF = this._writableState.finished = true;

    this._destroy(err, (err) => {
        if (err) {
            this._readableState.errorEmitted = true;
            this._writableState.errorEmitted = true;
            this.emit('error', err);
        }
        if (this._writableEmitClose)
            this.emit('close');
    });
    return this;
}

class Transform extends Duplex {
    constructor(options) {
        if (!options)
            options = {};

        super(options);
        if (options && options.transform)
            this._transform = options.transform.bind(this);
        if (options && options.flush)
            this._flush = options.flush.bind(this);
    }

    _read() {
        if (!this._transformChunk || this.transforming) {
            this._transformNeeded = true;
            return;
        }
        this._trasformNeeded = false;

        this.transforming = true;
        this._transform(this._transformChunk, this._transformEncoding, (err, data) => {
            if (data !== undefined)
                this.push(data);
            this.transforming = false;

            this._transformChunk = null;
            this._transformCallback(err);
        })
    }

    _write(chunk, encoding, callback) {
        this._transformChunk = chunk;
        this._transformEncoding = encoding;
        this._transformCallback = callback;
        if (this._transformNeeded)
            this._read();
    }

    _final(callback) {
        if (this._flush)
            this._flush(callback)
        else
            process.nextTick(callback);
    }
}

class PassThrough extends Transform {
    constructor(options) {
        super(options);
    }

    _transform(chunk, encoding, callback) {
        process.nextTick(callback, null, chunk);
    }
}

function noop() { }

function isRequest(stream) {
    return stream.setHeader && typeof stream.abort === 'function';
}

function once(callback) {
    let called = false;
    return function (err) {
        if (called) return;
        called = true;
        callback.call(this, err);
    };
}

function finished(stream, opts, callback) {
    if (typeof opts === 'function') return eos(stream, null, opts);
    if (!opts) opts = {};

    callback = once(callback || noop);

    const ws = stream._writableState;
    const rs = stream._readableState;
    let readable = opts.readable || (opts.readable !== false && stream.readable);
    let writable = opts.writable || (opts.writable !== false && stream.writable);

    const onfinish = () => {
        writable = false;
        if (!readable) callback.call(stream);
    };

    const onend = () => {
        readable = false;
        if (!writable) callback.call(stream);
    };

    const onerror = (err) => {
        callback.call(stream, err);
    };

    const onclose = () => {
        if (readable && !stream._readableEOF)
            return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
        if (writable && !stream._writableEOF)
            return callback.call(stream, new ERR_STREAM_PREMATURE_CLOSE());
    };

    const onrequest = () => {
        stream.req.on('finish', onfinish);
    };

    if (isRequest(stream)) {
        stream.on('complete', onfinish);
        stream.on('abort', onclose);
        if (stream.req) onrequest();
        else stream.on('request', onrequest);
    }

    stream.on('end', onend);
    stream.on('finish', onfinish);
    if (opts.error !== false) stream.on('error', onerror);
    stream.on('close', onclose);

    return function () {
        stream.removeListener('complete', onfinish);
        stream.removeListener('abort', onclose);
        stream.removeListener('request', onrequest);
        if (stream.req) stream.req.removeListener('finish', onfinish);
        stream.removeListener('finish', onfinish);
        stream.removeListener('end', onend);
        stream.removeListener('error', onerror);
        stream.removeListener('close', onclose);
    };
}

function pipeline(...streams) {
    // Streams should never be an empty array. It should always contain at least
    // a single stream. Therefore optimize for the average case instead of
    // checking for length === 0 as well.
    if (typeof streams[streams.length - 1] !== 'function')
        throw new ERR_INVALID_CALLBACK();
    let callback = streams.pop();

    if (Array.isArray(streams[0])) streams = streams[0];
    if (streams.length < 2)
        throw new ERR_MISSING_ARGS('streams');

    let destroyed = false;
    function destroyAll(error) {
        if (destroyed)
            return;
        destroyed = true;
        for (let i = 0; i < streams.length; i++) {
            let stream = streams[i];
            // request.destroy just do .end - .abort is what we want
            if (isRequest(stream)) return stream.abort();
            if (typeof stream.destroy === 'function') return stream.destroy();
        }
        callback(error);
    }

    let error;
    const destroys = streams.map(function (stream, i) {
        const reading = i < streams.length - 1;
        const writing = i > 0;
        return destroyer(stream, reading, writing, function (err) {
            if (!error) error = err;
            if (err) destroys.forEach(call);
            if (reading) return;
            destroys.forEach(call);
            callback(error);
        });
    });

    for (let i = 0; i < streams.length; i++)
        finished(stream, { readable: i < streams.length - 1, writable: i > 0 }, (err) => {
            destroyAll(err);
        });
    for (let i = 0; i < streams.length - 1; i++)
        streams[i].pipe(streams[i + 1]);
}

// *** legacy stream
function Stream() {
    EventEmitter.call(this);
}
Stream.prototype = Object.create(EventEmitter.prototype);

Stream.prototype.pipe = function (dest, options) {
    var source = this;

    function ondata(chunk) {
        if (dest.writable && dest.write(chunk) === false && source.pause) {
            source.pause();
        }
    }

    source.on('data', ondata);

    function ondrain() {
        if (source.readable && source.resume) {
            source.resume();
        }
    }

    dest.on('drain', ondrain);

    // If the 'end' option is not supplied, dest.end() will be called when
    // source gets the 'end' or 'close' events.  Only dest.end() once.
    if (!dest._isStdio && (!options || options.end !== false)) {
        source.on('end', onend);
        source.on('close', onclose);
    }

    var didOnEnd = false;
    function onend() {
        if (didOnEnd) return;
        didOnEnd = true;

        dest.end();
    }


    function onclose() {
        if (didOnEnd) return;
        didOnEnd = true;

        if (typeof dest.destroy === 'function') dest.destroy();
    }

    // don't leave dangling pipes when there are errors.
    function onerror(er) {
        cleanup();
        if (EventEmitter.listenerCount(this, 'error') === 0) {
            throw er; // Unhandled stream error in pipe.
        }
    }

    source.on('error', onerror);
    dest.on('error', onerror);

    // remove all the event listeners that were added.
    function cleanup() {
        source.removeListener('data', ondata);
        dest.removeListener('drain', ondrain);

        source.removeListener('end', onend);
        source.removeListener('close', onclose);

        source.removeListener('error', onerror);
        dest.removeListener('error', onerror);

        source.removeListener('end', cleanup);
        source.removeListener('close', cleanup);

        dest.removeListener('close', cleanup);
    }

    source.on('end', cleanup);
    source.on('close', cleanup);

    dest.on('close', cleanup);
    dest.emit('pipe', source);

    // Allow for unix-like usage: A.pipe(B).pipe(C)
    return dest;
};

module.exports = Stream;
Stream.Stream = Stream;
Stream.Readable = Readable;
Stream.Writable = Writable;
Stream.Duplex = Duplex;
Stream.Transform = Transform;
Stream.PassThrough = PassThrough;
Stream.finished = finished;
Stream.pipeline = pipeline;