'use strict';

let native = require('native');
let config = native.lowsysConfig();

module.exports = {
    codeMAC = config.codeMAC,
    partitions = {},
    setSettings = native.setSettings,
    himemWrite = native.himemWrite,
    himemRead = native.himemRead,
    sdcardFormat = native.sdcardFormat
};

Object.defineProperty(module.exports, 'status', {
    enumerable: true,
    get: function () {
        return native.getStatus();
    }
});
Object.defineProperty(module.exports, 'settings', {
    enumerable: true,
    get: function () {
        return native.getSettings();
    }
});

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
if(config.himemLength)
    module.exports.partitions[sdcard] = {length: config.himemLength};