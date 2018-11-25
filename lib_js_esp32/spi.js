'use strict';

/**
 * Module for SPI master, and after implemented, SPI slave interfaces
 * @module spi
 */

let native = require('native');
let gpio = require('gpio');

/** A SPI master interface ***/
class SPI {
    /**
     * Creates a SPI master interface.
     *
     * Trying to create more than 2 results in an exception, as the hardware does not support more.
     * Destroy them explicitly with distroy() when they are no longer in use.
     *
     * @param {Object} options The options
     * @param {Number} [options.mode=0] 0: capture data on first edge, the rest state of the clock (between frames) is low /
     *                          1: capture data on second edge, the rest state of the clock (between frames) is low /
     *                          2: capture data on first edge, the rest state of the clock (between frames) is high /
     *                          3: capture data on second edge, the rest state of the clock (between frames) is high
     * @param {Number} [options.clockHz=500000] speed of link in Hz
     * @param {Number} options.pinSCLK clock signal pin
     * @param {Number} [options.pinMISO] master to slave/output pin
     * @param {Number} [options.pinMOSI] slave to master/input pin
     */
    constructor(options, optionsOld) {
        if (optionsOld)             // backwards compat, will be removed
            options = optionsOld;   // end of 2019

        this._index = native.initPeripherial(1, options);
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
     * Acquires the given pin to use it as chip select pin during transfers.
     * Sets the pin level to high. When transfering data with this chip select pin
     * the pin level goes low during transfer.
     * 
     * @param {Number} pinCS chip select pin
     * @returns {SPI} interface itself, to chain call other methods
     */
    addCS(pinCS) {
        gpio.pins[pinCS].setType(gpio.OUTPUT).setValue(1);
        return this;
    }
    /**
     * Allows the program to use the given pin differently.
     * Sets the pin level to input.
     *
     * @param {Number} pinCS chip select pin
     * @returns {SPI} interface itself, to chain call other methods
     */
    removeCS(pinCS) {
        gpio.pins[pinCS].setType(gpio.INPUT);
        return this;
    }

    /**
     * Callback which is called when a transfer completed.
     *
     * @callback SPITransferCallback
     * @param {?Error} err optional error. If not null, the next parameters are not set
     * @param {Buffer} [data] received from slave. Is of same length as the data sent to the slave
     */

    /**
     * Transfers data to and from the slave.
     *
     * @param {Number} [pinCS] chip select pin
     * @param {Buffer} data data to send to slave
     * @param {SPITransferCallback} [callback] called when the transfer is completed
     */
    transfer(pinCS, data, callback) {
        if (this._ref) {
            this._pipe.push([pinCS, data, callback]);
            return;
        }

        if (pinCS && typeof pinCS !== 'number') {
            callback = data;
            data = pinCS;
            pinCS = null;
        }
        if (!data) {
            callback(null);
            if (this._pipe.length) {
                let entry = this._pipe.shift();
                this.transfer(entry[0], entry[1], entry[2]);
            }
            return;
        }

        if (this._holdRef)
            native.runRef(1);
        this._ref = true;

        let dataOut = new Buffer(data.length);
        native.transferPeripherial(this._index, pinCS, data, dataOut, (err) => {
            if(callback)
                callback(err, dataOut);

            if (this._holdRef)
                native.runRef(-1);
            this._ref = false;

            if (this._pipe.length) {
                let entry = this._pipe.shift();
                this.transfer(entry[0], entry[1], entry[2]);
            }
        });
    }

    /**
     * Calls the callback as soon as the last transfer is completed.
     *
     * @param {SPITransferCallback} callback called when the last transfer is completed
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
     * @returns {SPI} interface itself, to chain call other methods
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
     * @returns {SPI} interface itself, to chain call other methods
     */
    unref() {
        if (this._ref && this._holdRef)
            native.runRef(-1);
        this._holdRef = false;
        return this;
    }
}

module.exports = {
    SPI
};