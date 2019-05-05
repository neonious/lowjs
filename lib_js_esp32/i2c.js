'use strict';

let native = require('native');

/**
 * Module for I2C master, and after implemented, I2C slave interfaces
 * @module i2c
 */

class I2C {
    /**
     * Creates a I2C master interface.
     *
     * Destroy explicitly with destroy() when the interface is no longer in use.
     *
     * @param {Object} options The options
     * @param {Number} [options.clockHz=100000] speed of link in Hz
     * @param {Number} options.pinSCL clock signal pin
     * @param {Number} options.pinSDC data pin
     */
    constructor(options, optionsOld) {
        // options:
        // clockHz
        // pinSDA
        // pinSCL

        if (optionsOld)             // backwards compat, will be removed
            options = optionsOld;   // end of 2019

        this._index = native.initPeripherial(2, options);
        this._holdRef = true;
        this._ref = false;
        this._pipe = [];
    }

    /**
     * Frees all resources of the interface, allowing the program to use the pins differently or
     * construct a new interface with other parameters.
     */
    destroy() {
        native.destroyPeripherial(this._index);
    }

    /**
     * Callback which is called when a transfer completed.
     *
     * @callback I2CTransferCallback
     * @param {?Error} err optional error. If not null, the next parameters are not set
     * @param {Buffer} [data] received from slave
     */

    /**
     * Does both the write and read phase of a transfer in one step.
     *
     * @param {Number} address the address of the I2C slave
     * @param {Buffer} data data to send to slave
     * @param {Number} bytesRead how many bytes should be read after sending the data to the slave
     * @param {I2CTransferCallback} [callback] called when the transfer is completed
     */
    transfer(address, data, bytesRead, callback) {
        if (this._ref) {
            this._pipe.push([address, data, bytesRead, callback]);
            return;
        }
        if (!data && !bytesRead) {
            callback(null);
            if (this._pipe.length) {
                let entry = this._pipe.shift();
                this.transfer(entry[0], entry[1], entry[2], entry[3]);
            }
            return;
        }

        if (this._holdRef)
            native.runRef(1);
        this._ref = true;

        let dataOut = bytesRead ? new Buffer(bytesRead) : null;
        native.transferPeripherial(this._index, address, data, dataOut, (err) => {
            if(callback)
                callback(err, err ? null : dataOut);

            if (this._holdRef)
                native.runRef(-1);
            this._ref = false;

            if (this._pipe.length) {
                let entry = this._pipe.shift();
                this.transfer(entry[0], entry[1], entry[2], entry[3]);
            }
        });
    }

    /**
     * Write data to the slave.
     *
     * @param {Number} address the address of the I2C slave
     * @param {Buffer} data data to send to slave
     * @param {I2CTransferCallback} [callback] called when the transfer is completed
     */
    write(address, data, callback) {
        transfer(address, data, 0, callback);
    }

    /**
     * Read data from the slave.
     *
     * @param {Number} address the address of the I2C slave
     * @param {Number} bytesRead how many bytes should be read after sending the data to the slave
     * @param {I2CTransferCallback} [callback] called when the transfer is completed
     */
    read(address, bytesRead, callback) {
        transfer(address, null, bytesRead, callback);
    }

    /**
     * Calls the callback as soon as the last transfer is completed.
     *
     * @param {I2CTransferCallback} callback called when the last transfer is completed
     */
    flush(callback) {
        if (this._ref)
            this._pipe.push([null, null, callback]);
        else
            callback(null);
    }

    /**
     * Tells the interface to keep the program running when a transfer is taking place.
     * This is the default.
     *
     * @returns {I2C} interface itself, to chain call other methods
     */
    ref() {
        if (this._ref && !this._holdRef)
            native.runRef(1);
        this._holdRef = true;
        return this;
    }
    /**
     * Tells the interface to not keep the program running when a transfer is taking place,
     * but there is nothing else to do.
     *
     * @returns {I2C} interface itself, to chain call other methods
     */
    unref() {
        if (this._ref && this._holdRef)
            native.runRef(-1);
        this._holdRef = false;
        return this;
    }
}

module.exports = {
    I2C
};