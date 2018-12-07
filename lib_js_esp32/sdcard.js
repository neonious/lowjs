'use strict';

let native = require('native');

exports.mount = (cb) => {
    native.sdcardMount(false, cb);
}

exports.format = (cb) => {
    native.sdcardMount(true, cb);
}