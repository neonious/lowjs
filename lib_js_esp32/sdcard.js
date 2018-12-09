'use strict';

/**
 * Methods to control SD cards. The use is optional, because
 * the support of SD cards is built into low.js. To use an SD card, all you
 * need to do is enable it in the low.js settings.
 * @module sdcard
 */

let native = require('native');

/**
 * Callback which is called when the sd card is mounted or formatted
 *
 * @callback SDCardMountFormatCallback
 * @param {?Error} err optional error
 */


/**
 * Re-mounts the SD card. Do this if a read or write to the sd card fails.
 *
 * @param {SDCardMountFormatCallback} [callback] called when the sd card is re-mounted
 */
exports.mount = (cb) => {
    native.sdcardMount(false, cb);
}

/**
 * Formats the SD card
 *
 * @param {SDCardMountFormatCallback} [callback] called when the sd card is formatted
 */
exports.format = (cb) => {
    native.sdcardMount(true, cb);
}