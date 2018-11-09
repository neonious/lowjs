
'use strict';

let native = require('native');

exports.ONESHOT = 0;
exports.RESTART = 1;

exports.EVENT_1 = 1;
exports.EVENT_2 = 2;
exports.EVENT_3 = 4;
exports.EVENT_4 = 8;
exports.EVENT_5 = 16;
exports.EVENT_6 = 32;
exports.EVENT_7 = 64;
exports.EVENT_8 = 128;

exports.send = function (flags, pins, eventNanoSecs, callback) {
    let numPins = pins.length;
    let numEvents = eventNanoSecs.length;

    let size = 1 + numPins * 3 + numEvents * 4;
    let buf = new Buffer(size);

    buf.writeUInt8((flags | 0) + ((numPins - 1) << 2) + ((numEvents - 1) << 5), 0);
    for (let i = 0; i < numPins; i++) {
        buf.writeUInt8(pins[i].index, i * 3 + 1);
        buf.writeUInt8(pins[i].setEvents, i * 3 + 2);
        buf.writeUInt8(pins[i].clearEvents, i * 3 + 3);
    }
    let pos = 1 + numPins * 3;
    for (let i = 0; i < numEvents; i++)
        buf.writeUInt32LE(eventNanoSecs[i], pos + i * 4);

    native.signalSend(buf, callback);
}

exports.clear = function () {
    native.signalSend(null);
}