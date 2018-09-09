let stream = require('stream');
let native = require('native');

class I2C {
    constructor(index, options) {
        // options:
        // clockHz
        // pinMISO
        // pinMOSI

        index = index | 0;
        if (index <= 0 || index > 4)
            throw new RangeError("I2C channel " + index + " does not exist");

        index += 3;    // 4-7
        this._index = index;

        native.initPeripherial(index, options);
    }

    destroy() {
        native.destroyPeripherial(this._index);
    }

    bind(address) {
        return new I2CChannel(this, address);
    }
}

class I2CChannel extends stream.Duplex {
    constructor(i2c, address) {
        let index = i2c._index | (address << 8);

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
            }
        });
    }
}

module.exports = {
    I2C,
    I2CChannel
};