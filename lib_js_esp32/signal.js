'use strict';

/**
 * Module to send control signals. Currently only on neonious one.
 * 
 * Allows to send an signal with a length of up to 2 ^ 32 nanoseconds (a bit over 4 seconds)
 * to up to 8 pins, with up to 6 defined points in time(events) when the pins can individually
 * be turned on or off. The points in time are defined in nanoseconds, but final the resolution
 * is 33.333... nanoseconds.
 * 
 * @module signal
 */

let native = require('native');

/**
 * If set as flag in call to send(), the signal only runs once.
 */
exports.ONESHOT = 0;

/**
 * If set as flag in call to send(), the signal runs in a loop till another signal is sent or clear() is called.
 * Signal restarts after highest defined event, so in this the highest event should be the latest event.
 */
exports.RESTART = 1;

/**
 * First, lowest, event
 */
exports.EVENT_1 = 1;

exports.EVENT_2 = 2;
exports.EVENT_3 = 4;
exports.EVENT_4 = 8;
exports.EVENT_5 = 16;
exports.EVENT_6 = 32;
exports.EVENT_7 = 64;

/**
 * Highest possible event
 */
exports.EVENT_8 = 128;


/**
 * Callback which is called when a signal is stopped, either because it was completed or interrupted with another signal
 *
 * @callback SignalDoneCallback
 * @param {?Error} err optional error. If not null, the next parameters are not set
 * @param {boolean} [isCompleted] true, if signal completed, false if signal was interrupted
 */

/**
 * Sends the given signal. If a signal is already running, it is stopped, the callback given
 * to send() for the last signal is called, and the new signal is sent.
 *
 * @param {(ONESHOT|RESTART)} flags shall the signal repeat itself?
 * @param {Array} pins array with one object per pin used
 * @param {Number} pins[].index pin number
 * @param {(EVENT_1|EVENT_2|EVENT_3|EVENT_4|EVENT_5|EVENT_6|EVENT_7|EVENT_8)} pins[].setEvents binary OR of events which set the pin to high level
 * @param {(EVENT_1|EVENT_2|EVENT_3|EVENT_4|EVENT_5|EVENT_6|EVENT_7|EVENT_8)} pins[].clearEvents binary OR of events which set the pin to low level
 * @param {Number[]} eventNanoSecs array with numbers of the points in time in the unit nanoseconds of the used events. The first number is for EVENT_1, the second for EVENT_2, and so on
 * @param {SignalDoneCallback} [callback] called when the signal is stopped, either because it was completed or interrupted with another signal
 */
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

/**
 * Stops any still running signal. The callback given to send() is called.
 */
exports.clear = function () {
    native.signalSend(null);
}