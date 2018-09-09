'use strict';

// TODO: allow 3 different frequencies

let native = require('native');
let signal = require('signal');

let EventEmitter = require('events').EventEmitter;

// pins
exports.LED_RED = 1;
exports.LED_GREEN = 2;
exports.BUTTON = 27;

// types
exports.INPUT = 0;
exports.INPUT_PULLUP = 1;
exports.INPUT_PULLDOWN = 2;
exports.OUTPUT = 3;
exports.OUTPUT_OPENDRAIN = 4;

// values
exports.LOW = 0;
exports.HIGH = 1;

exports.DIGITAL = 0;
exports.ANALOG = 1;

// ***** ADC/GPIO GET HANDLING *****
let adcQueue = [], gpioQueue = [];
let adcRunning = false, gpioRunning = false;

function handleADC(index, callback) {
    if (adcRunning) {
        adcQueue.push([index, callback]);
        return;
    }
    adcRunning = true;

    native.gpioGetValues(index, exports.ANALOG, (value) => {
        callback(null, value);

        adcRunning = false;
        if (adcQueue.length) {
            let elem = adcQueue.shift();
            handleADC(elem[0], elem[1]);
        }
    });
}

function handleGPIO(index, callback) {
    if (gpioRunning) {
        gpioQueue.push([index, callback]);
        return;
    }
    gpioRunning = true;

    native.gpioGetValues(index, exports.DIGITAL, (value) => {
        // Only the relevant bit is set correctly
        callback(null, (value & (1 << (index - 1))) ? 1 : 0);

        gpioRunning = false;
        if (gpioQueue.length) {
            let elem = gpioQueue.shift();
            handleGPIO(elem[0], elem[1]);
        }
    });
}

// ***** PWM HANDLING *****
let pwmPins = [], pwmDuty = [];
let signalNanoSecs = 10000000; // 100 Hz

function sendPWMSignal() {
    if (pwmPins.length == 0) {
        signal.clear();
        return;
    }

    let pins = [];
    let eventNanoSecs = [];

    let pin = 1, setEvent = 1 << pwmPins.length;
    for (let i = 0; i < pwmPins.length; i++) {
        pins.push({
            index: pwmPins[i],
            setEvents: setEvent,
            clearEvents: pin
        });
        eventNanoSecs.push((pwmDuty[i] * signalNanoSecs) | 0);
        pin <<= 1;
    }
    eventNanoSecs.push(0);
    eventNanoSecs.push(signalNanoSecs | 0);

    signal.send(signal.RESTART, pins, eventNanoSecs);
}

function disablePWM(index) {
    let val = pwmPins.indexOf(index);
    if (val >= 0) {
        pwmPins.splice(val, 1);
        pwmDuty.splice(val, 1);
        sendPWMSignal();
    }
}

function setPWM(index, value) {
    let val = pwmPins.indexOf(index);
    if (val >= 0) {
        if (pwmDuty[val] != value) {
            pwmDuty[val] = value;
            sendPWMSignal();
        }
    } else {
        if (pwmPins.length == 6)
            throw new RangeError('more than 6 PWM pins are not supported');

        pwmPins.push(index);
        pwmDuty.push(value);
        sendPWMSignal();
    }
}

let values = 0;

function GPIOPin(index) {
    let pinType = index == exports.LED_RED || index == exports.LED_GREEN ? exports.OUTPUT : exports.INPUT;
    let isPWM = false;
    let rise = false, fall = false;

    this.setType = (type) => {
        let setType = type;
        if (type < 3) {
            if (isPWM)
                disablePWM(index);

            if (rise)
                setType |= 8;
            if (fall)
                setType |= 16;
        }
        native.gpioSetType(index, setType);
        pinType = type;
    }
    this.getType = () => {
        return pinType;
    }

    this.setValue = (value) => {
        if (pinType < 3)
            throw new RangeError("pin " + index + " is set to input");
        if (value < 0 || value > 1)
            throw new RangeError("can only set pin to a value between 0 and 1");

        if (value > 0.0 && value < 1.0)  // PWM
            setPWM(index, value);
        else {
            disablePWM(index);

            if (value)
                values |= 1 << (index - 1);
            else
                values &= ~(1 << (index - 1));
            native.gpioSetValues(values);
        }
    }
    this.getValue = (flags, callback) => {
        if (typeof flags === 'function')
            handleGPIO(index, flags);
        else if (flags & exports.ANALOG)
            handleADC(index, callback);
        else
            handleGPIO(index, callback);
    }

    this.on('newListener', (event, listener) => {
        let change = false;
        if (event == 'rise' && !rise) {
            native.runRef(1);
            rise = true;
            change = true;
        }
        if (event == 'fall' && !fall) {
            native.runRef(1);
            fall = true;
            change = true;
        }

        if (change && pinType < 3)
            this.setType(pinType);
    });
    this.on('removeListener', (event, listener) => {
        let change = false;
        if (event == 'rise' && !this.listenerCount('rise') && rise) {
            native.runRef(-1);
            rise = false;
            change = true;
        }
        if (event == 'fall' && !this.listenerCount('fall') && fall) {
            native.runRef(-1);
            fall = false;
            change = true;
        }

        if (change && pinType < 3)
            this.setType(pinType);
    });
}
GPIOPin.prototype = Object.create(EventEmitter.prototype);

let internalPins = [];

native.gpioSetCallback((index, rise) => {
    if (!internalPins[index])
        return;

    if (rise)
        internalPins[index].emit('rise');
    else
        internalPins[index].emit('fall');
});

exports.pins = [];
for (var i = 1; i <= 27; i++) {
    exports.pins[i] = internalPins[i] = new GPIOPin(i);
}

exports.setFrequency = (frequency) => {
    let nanoSecs = 1000000000 / frequency;
    if (signalNanoSecs != nanoSecs) {
        signalNanoSecs = nanoSecs;
        sendPWMSignal();
    }
}
exports.getFrequency = () => {
    return 1000000000 / signalNanoSecs;
}

exports.setPWMNanoSecs = (nanoSecs) => {
    if (signalNanoSecs != nanoSecs) {
        signalNanoSecs = nanoSecs;
        sendPWMSignal();
    }
}
exports.getPWMNanoSecs = () => {
    return signalNanoSecs;
}

exports.setValues = (bits) => {
    values = bits;
    native.gpioSetValues(bits);
}

exports.getValues = (callback) => {
    native.gpioGetValues(0, 0, callback);
}
