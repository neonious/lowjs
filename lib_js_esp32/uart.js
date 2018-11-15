/**
 * Module for UART interface
 * @module uart
 */

let stream = require('stream');
let native = require('native');

/**
 * A UART interface, implemented as Duplex stream.
 *
 * Check the Node.JS Duplex stream documentation on information how
 * to read and write from the stream.
 * 
 * Call resume() if you are not interested in the received data so it does not
 * fill up memory.
 * 
 * @extends net.Duplex
 */
class UART extends stream.Duplex {
    /**
     * Creates a UART interface.
     *
     * Trying to create more than 2 (neonious one) or 3 (ESP32-WROVER) results in an exception,
     * as the hardware does not support more. Destroy them explicitly with distroy() when they
     * are no longer in use.
     *
     * @param {Object} options The options
     * @param {Number} [options.baud=9600] speed of link in baud
     * @param {Number} [options.stopBits=1] number of stop bits (1 or 2)
     * @param {Number} [options.dataBits=8] number of data bits (7 or 8)
     * @param {String} [options.parity] either "odd", "even", or do not set for no parity
     * @param {Number} [options.pinRX] receive pin
     * @param {Number} [options.pinTX] transmit pin
     */
    constructor(options, optionsOld) {
        if (optionsOld)             // backwards compat, will be removed
            options = optionsOld;   // end of 2019

        this._index = native.initPeripherial(0, options);
        this.ref();

        super({
            allowHalfOpen: false,
            read(size) {
                let buf = new Buffer(size);
                native.transferPeripherial(this._index, 0, null, buf, (err, bytesRead) => {
                    if (err) {
                        this.destroy(err);
                        return;
                    }
                    this.push(bytesRead != size ? buf.slice(0, bytesRead) : buf);
                });
            },
            write(chunk, encoding, callback) {
                native.transferPeripherial(this._index, 1, chunk, null, (err, bytesWritten) => {
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
                native.destroyPeripherial(this._index);
                callback(err);
            }
        });
    }

    /**
     * Callback which is called when a transfer completed.
     *
     * @callback UARTTransferCallback
     * @param {?Error} err optional error. If not null, the next parameters are not set
     */

    /**
     * Calls the callback as soon as the last transfer is completed.
     *
     * @param {UARTTransferCallback} callback called when the last transfer is completed
     */
    flush(callback) {
        native.flushPeripherial(this._index, callback);
    }

    /**
     * Tells the interface to keep the program running when a transfer is taking place.
     * This is the default.
     *
     * @returns {UART} interface itself, to chain call other methods
     */
    ref() {
        if (!this._ref && !this.destroyed) {
            native.runRef(1);
            this._ref = true;
        }
        return this;
    }
    /**
     * Tells the interface to not keep the program running when a transfer is taking place,
     * but there is nothing else to do.
     *
     * @returns {UART} interface itself, to chain call other methods
     */
    unref() {
        if (this._ref && !this.destroyed) {
            native.runRef(-1);
            this._ref = false;
        }
        return this;
    }
}

module.exports = {
    UART
};