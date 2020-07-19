'use strict';

const events = require('events');
const dns = require('dns');
const native = require('native');

const { isUint8Array } = require('internal/util/types');
const errors = require('internal/errors');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_MISSING_ARGS,
  ERR_SOCKET_ALREADY_BOUND,
  ERR_SOCKET_BAD_BUFFER_SIZE,
  ERR_SOCKET_BAD_PORT,
  ERR_SOCKET_BUFFER_SIZE,
  ERR_SOCKET_CANNOT_SEND,
  ERR_SOCKET_DGRAM_NOT_RUNNING,
  ERR_INVALID_FD_TYPE
} = errors.codes;

class Socket extends events.EventEmitter {
    constructor(options, listener) {
        if (options === null || typeof options !== 'object')
            options = {type: options};

        this._ref = true;
        super();

        this._options = Object.assign({}, options);
        if(!this._options.lookup)
            this._options.lookup = dns.lookup;
        this._options.reuseAddr = !!this._options.reuseAddr;

        // TODO:
        //recvBufferSize <number> - Sets the SO_RCVBUF socket value.
        //sendBufferSize <number> - Sets the SO_SNDBUF socket value.
    }

    close(callback) {
        if (this.destroyed)
            return this;

        this.destroyed = true;
        if(callback)
            this.once('close', callback);

        if(this._socketFD === undefined || this._tryConnect)
            return this;

        native.close(this._socketFD, (err) => {
            if(err) {
                this.destroyed = false;
                this.emit('error', err);
                return;
            }

            delete this._socketFD;
            this.unref();

            this.emit('close');
        });
        return this;
    }

    address() {
        return this._address;
    }

    send(buffer, offset, length, port, address, callback) {
        if (address || (port && typeof port !== 'function')) {
          buffer = sliceBuffer(buffer, offset, length);
        } else {
          callback = port;
          port = offset;
          address = length;
        }
      
        if (Array.isArray(buffer)) {
            if (!(buffer = fixBufferList(buffer))) {
                throw new ERR_INVALID_ARG_TYPE('buffer list arguments',
                                               ['Buffer', 'string'], buffer);
              }
            buffer = Buffer.concat(buffer);
        } else if (typeof buffer === 'string') {
            buffer = Buffer.from(buffer);
        } else if (!isUint8Array(buffer)) {
          throw new ERR_INVALID_ARG_TYPE('buffer',
                                         ['Buffer', 'Uint8Array', 'string'],
                                         buffer);
        }
      
        port = port >>> 0;
        if (port === 0 || port > 65535)
          throw new ERR_SOCKET_BAD_PORT(port);
      
        // Normalize callback so it's either a function or undefined but not anything
        // else.
        if (typeof callback !== 'function')
          callback = undefined;
      
        if (typeof address === 'function') {
          callback = address;
          address = undefined;
        } else if (address && typeof address !== 'string') {
          throw new ERR_INVALID_ARG_TYPE('address', ['string', 'falsy'], address);
        }

        if (this.destroyed)
            throw new Error('Socket already closed.');
        if (this._socketFD === undefined && !this._tryConnect)
            this.bind(0);

        if (this._tryConnect || this._writing) {
            enqueue(this, this.send.bind(this, buffer, port, address, callback));
            return;
        }

        this._writing = true;
        if (!address || native.isIP(address))
            this._send(buffer,
                address ? address : (this._options.type == 'udp4' ? '127.0.0.1' : '::1'),
                port, callback);
        else
            this._options.lookup(address, (err, host, family) => {
                this._send(buffer, host, port, callback);
            });
    }
    _send(buffer, address, port, callback) {
        native.send(this._socketFD, buffer, address, port, (err) => {
            this._writing = false;
            if (!this.destroyed)
                onCanWrite.bind(this)();
            if(callback)
                callback(err);
        });
    }

    bind(port, address_, callback) {
        if (this.destroyed)
            throw new Error('Socket already closed.');
        if (this._socketFD !== undefined || this._tryConnect || this.destroyed)
            throw new Error('Socket already bound/binding.');
        this._tryConnect = true;

        let address;
        if (port !== null && typeof port === 'object') {
            if(port.fd !== undefined)
                throw new Error('bind to fd not supported with low.js, as bad pactrice');

            address = port.address || '';
            port = port.port | 0;
        } else {
            address = typeof address_ === 'function' ? '' : address_;
            port = port | 0;
            // 'exclusive' argument not used, we do not support cluster with low.js by design
        }

        let family = !address ? 0 : native.isIP(address);
        if (!address || family)
            this._bind(address ? address : (this._options.type == 'udp4' ? '0.0.0.0' : '::'),
                port, callback);
        else
            this._options.lookup(address, (err, host) => {
                if (err) {
                    this._tryConnect = false;
                    onListenError.bind(this)();
                    this._updateRef();

                    this.emit('error', err);
                    return;
                }
                if (this.destroyed) {
                    this.connecting = false;
                    this._updateRef();
                    return;
                }

                this._bind(host, port, callback);
            });
    }
    _bind(address, port, callback) {
        let family = this._options.type == 'udp4' ? 4 : 6;

        this._updateRef();
        native.bind(address, port, family,
            this._options.reuseAddr, (err, fd, port) => {
            if(err) {
                this._tryConnect = false;
                onListenError.bind(this)();
                this._updateRef();

                this.emit('error', err);
                return;
            }
            if (this.destroyed) {
                onListenError.bind(this)();
                native.close(fd, (err) => {
                    this._tryConnect = false;
                    this._updateRef();
                    if (err)
                        this.emit('error', err);
                });
                return;
            }

            this._tryConnect = false;
            this._socketFD = fd;
            this._address = {family: 'IPv' + family, address, port};
            this._updateRef();

            onCanWrite.bind(this)();
            this.emit('listening');
            if (callback)
                callback(null);
         }, (err, msg, rinfo) => {
            if(err)
                this.emit('error', err);
            else
                this.emit('message', msg, rinfo);
        });
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
        if (this._ref && (this._tryConnect || this.destroyed || this._socketFD !== undefined)) {
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

/*
    function setBroadcast(flag) {
    }

    function getRecvBufferSize() {
    }

    function getSendBufferSize() {
    }

    function setRecvBufferSize(size) {
    }

    function setSendBufferSize(size) {
    }

    function addMembership(multicastAddress, multicastInterface) {
    }

    function dropMembership(multicastAddress, multicastInterface) {
    }

    function setMulticastInterface(multicastInterface) {
    }

    function setMulticastLoopback(flag) {
    }

    function setMulticastTTL(ttl) {
    }

    function setTTL(ttl) {
    }
*/
}

function createSocket(options, callback) {
    if(typeof options === 'string')
        options = {type: options};

    let socket = new Socket(options);
    if(callback)
        socket.on('message', callback);

    return socket;
}

module.exports = {
    Socket,
    createSocket
};

// From node
function enqueue(self, toEnqueue) {
    // If the send queue hasn't been initialized yet, do it, and install an
    // event handler that flushes the send queue after binding is done.
    if (self._queue === undefined)
        self._queue = [];
    self._queue.push(toEnqueue);
  }
  
  function onCanWrite() {
    if(this._queue && this._queue.length)
        this._queue.shift()();
  }
  
  function onListenError(err) {
    this._queue = undefined;
  }

  function sliceBuffer(buffer, offset, length) {
    if (typeof buffer === 'string') {
      buffer = Buffer.from(buffer);
    } else if (!isUint8Array(buffer)) {
      throw new ERR_INVALID_ARG_TYPE('buffer',
                                     ['Buffer', 'Uint8Array', 'string'], buffer);
    }
  
    offset = offset >>> 0;
    length = length >>> 0;
  
    return buffer.slice(offset, offset + length);
  }
  
  
  function fixBufferList(list) {
    const newlist = new Array(list.length);
  
    for (var i = 0, l = list.length; i < l; i++) {
      var buf = list[i];
      if (typeof buf === 'string')
        newlist[i] = Buffer.from(buf);
      else if (!isUint8Array(buf))
        return null;
      else
        newlist[i] = buf;
    }
  
    return newlist;
  }
