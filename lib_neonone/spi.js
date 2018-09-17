let stream = require('stream');
let native = require('native');

class SPI extends stream.Duplex {
    constructor(index, options) {
        // options:
        // mode
        // clockHz
        // pinCS
        // pinSCLK
        // pinMISO
        // pinMOSI

        index = index | 0;
        if (index <= 0 || index > 2)
            throw new RangeError("SPI channel " + index + " does not exist");

        index += 1;    // 2-3
        this._index = index;

        this.ref();
        native.initPeripherial(index, options);

        super({
            allowHalfOpen: false,
            read(size) {
                let buf = new Buffer(size);
                native.readPeripherial(index, buf, (err, bytesRead) => {
                    if (err) {
                        this.destroy(err);
                        return;
                    }
                    this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                });
            },
            write(chunk, encoding, callback) {
                native.writePeripherial(index, chunk, (err, bytesWritten) => {
                    if (err) {
                        this.destroy(err);
                        return;
                    }

                    if (chunk.length != bytesWritten) {
                        this._write(chunk.slice(bytesWritten), encoding, callback);
                        return;
                    }
                    callback();
                });
            },
            final(callback) {
                this._destroy(null, callback);
            },
            destroy(err, callback) {
                if (this._destroyed) {
                    callback(err);
                    return;
                }
                this._destroyed = true;

                if (this._ref) {
                    native.runRef(-1);
                    this._ref = false;
                }
                native.destroyPeripherial(index);
                callback(err);
            }
        });
    }

    flush(callback) {
        native.flushPeripherial(this._index, callback);
    }

    ref() {
        if (!this._ref && !this.destroyed) {
            native.runRef(1);
            this._ref = true;
        }
        return this;
    }
    unref() {
        if (this._ref && !this.destroyed) {
            native.runRef(-1);
            this._ref = false;
        }
        return this;
    }
}

module.exports = {
    SPI
};