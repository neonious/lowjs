'use strict';

let native = require('native');
let config = native.lowsysConfig();

/**
 * Methods to control low.js itself
 * @module lowsys
 */

module.exports = {
    /**
     * The 12-digit code/base MAC address of the device
     * @property {String}
     * @name codeMAC
     */
    codeMAC: config.codeMAC,

    partitions: {},
//    setSettings: native.setSettings,

    /**
     * Callback which is called when a transfer completed.
     *
     * @callback HimemTransferCallback
     * @param {?Error} err optional error
     */


    /**
     * Allows user program to write to the higher 4 MB of PSRAM
     * if ESP-WROVER-B is used. This memory is not used by low.js itself
     * because it cannot be accessed directly bei the ESP32 chip.
     * @function himemWrite
     * @param {Buffer} buffer the buffer with data
     * @param {Number} himemOffset the offset in high memory to start writing
     * @param {Number} [bufOffset=0] the offset in the buffer where to get the data
     * @param {Number} [length=buffer.length-bufOffset] the number of bytes to write
     * @param {HimemTransferCallback} callback the callback to call when the data is written
     */
    himemWrite: native.himemWrite,

    /**
     * Allows user program to read from the higher 4 MB of PSRAM
     * if ESP-WROVER-B is used. This memory is not used by low.js itself
     * because it cannot be accessed directly bei the ESP32 chip.
     * @function himemRead
     * @param {Buffer} buffer the buffer to fill
     * @param {Number} himemOffset the offset in high memory where to start reading
     * @param {Number} [bufOffset=0] the offset in the buffer where to start writing to
     * @param {Number} [length=buffer.length-bufOffset] the number of bytes to read
     * @param {HimemTransferCallback} callback the callback to call when the data is read
     */
    himemRead: native.himemRead
};

/**
 * An object holding status information
 * @property {String} status.eth Status of Ethernet interface
 * @property {String} status.wifi Status of Wifi interface
 * @property {String} status.time Status of requesting time from time server
 * @property {String} status.sdcard Status of sd card
 * @name status
 */

 /**
 * An object holding status information
 * @property {String} status.eth Status of Ethernet interface
 * @property {String} status.wifi Status of Wifi interface
 * @property {String} status.time Status of requesting time from time server
 * @property {String} status.sdcard Status of sd card
 * @name status
 */
Object.defineProperty(module.exports, 'status', {
    enumerable: true,
    get: function () {
        return native.getStatus();
    }
});

/*
Object.defineProperty(module.exports, 'settings', {
    enumerable: true,
    get: function () {
        return native.getSettings();
    }
});
*/

/**
 * An object holding information about all available parititons
 * @property {Number} partitions.flash.used Bytes used of flash partition
 * @property {Number} partitions.flash.size Total size of flash partition in bytes
 * @property {Number} [partitions.sdcard.used] Bytes used of sd card partition
 * @property {Number} [partitions.sdcard.size] Total size of sd card partition in bytes
 * @property {Number} [partitions.himem.size] Bytes of RAM not used by low.js and available via himemRead/himemWrite
 * @name partitions
 */
Object.defineProperty(module.exports.partitions, 'flash', {
    enumerable: true,
    get: function () {
        return native.statPartition(0);
    }
});
Object.defineProperty(module.exports.partitions, 'sdcard', {
    enumerable: true,
    get: function () {
        return native.statPartition(1);
    }
});
module.exports.partitions.himem = {size: config.himemLength};