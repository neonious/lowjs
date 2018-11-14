'use strict';

/**
 * Module to access pins for general purpose input/output. Also supports reading
 * pins voltage via ADC and settings pins via PWM.
 * 
 * @module gpio
 */

 // TODO: allow multiple frequencies for pwm (3 for neonious one, other for esp32)

let native = require('native');

// set below
let isNeoniousOne;
let signal;

let EventEmitter = require('events').EventEmitter;

/**
 * Pins set to this type are input pins.
 */
exports.INPUT = 0;

/**
 * Pins set to this type are input pins with the internal pullup resistor active.
 */
exports.INPUT_PULLUP = 1;

/**
 * Pins set to this type are input pins with the internal pulldown resistor active.
 */
exports.INPUT_PULLDOWN = 2;

/**
 * Pins set to this type are output pins.
 */
exports.OUTPUT = 3;

/**
 * Pins set to this type are output pins where level 1 is defined as not connected.
 * Great for implementing busses. To use these, you need to use an external pull-up.
 */
exports.OUTPUT_OPENDRAIN = 4;

/**
 * Low level = 0
 * @type {Number}
 */
exports.LOW = 0;

/**
 * High level = 1
 * @type {Number}
 */
exports.HIGH = 1;

/**
 * Flag for GPIOPin.getValue. Retrieves level of pin (exact 0 or 1).
 */
exports.DIGITAL = 0;

/**
 * Flag for GPIOPin.getValue. Retrieves voltage of pin via ADC (0 = 0V, 1 = board voltage).
 */
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

    native.gpioGetValues(index, exports.ANALOG, (err, value) => {
        callback(err, value);

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

    native.gpioGetValues(index, exports.DIGITAL, (err, valuesLo, valuesHi) => {
        // Only the relevant bit is set correctly
        if (index === null)
            callback(err, valuesLo, valuesHi);
        else
            callback(err, ((index < 32 ? valuesLo : valuesHi) & (1 << index)) ? 1 : 0);

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
    if (isNeoniousOne) {
        if (!signal)
            signal = require('signal');
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
    } else
        native.pwmConfig(signalNanoSecs, pwmPins, pwmDuty);
}

function disablePWM(index) {
    let val = pwmPins.indexOf(index);
    if (val >= 0) {
        if (isNeoniousOne) {
            pwmPins.splice(val, 1);
            pwmDuty.splice(val, 1);
        } else
            pwmPins[val] = -1;
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
        return;
    }

    if (!isNeoniousOne) {
        let val = pwmPins.indexOf(-1);
        if (val >= 0) {
            pwmPins[val] = index;
            pwmDuty[val] = value;
            sendPWMSignal();
            return;
        }

        if (pwmPins.length == 8)
            throw new RangeError('more than 8 PWM pins are not supported');
    } else if (pwmPins.length == 6)
            throw new RangeError('more than 6 PWM pins are not supported');

    pwmPins.push(index);
    pwmDuty.push(value);
    sendPWMSignal();
}

let valuesLo = 0, valuesHi = 0;

/**
 * @class
 * A pin of the device. Do not construct new objects, access the pins via gpio.pins[]. The pin
 * numbering for the neonious one is documented in the neonious one documentation. Please refer to it
 * for possible limitations of supported pins.
 * @extends events.EventEmitter
 * @fires rise
 * @fires fall
 */
function GPIOPin(index) {
    let pinType = index == exports.LED_RED || index == exports.LED_GREEN ? exports.OUTPUT : exports.INPUT;
    let isPWM = false;
    let rise = false, fall = false;

    /**
     * Sets the type of the pin.
     * At program start, all pins are set to INPUT, with exception of the LED pins on neonious ones. These are set to OUTPUT with level 0.
     *
     * @param {(INPUT|INPUT_PULLUP|INPUT_PULLDOWN|OUTPUT|OUTPUT_OPENDRAIN)} type the type
     * @returns {GPIOPin} pin itself, to chain call methods
     */
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
        return this;
    }
    /**
     * Returns the type of the pin.
     *
     * @returns {(INPUT|INPUT_PULLUP|INPUT_PULLDOWN|OUTPUT|OUTPUT_OPENDRAIN)} the type
     */
    this.getType = () => {
        return pinType;
    }

    /**
     * Sets the level of the pin or the pin to PWM with the given duty period if a value between 0 and 1 is given.
     * On neonious one, PWM is implemented with the signal module, so PWM and program defined signals cannot be used at the same time.
     *
     * @param {Number} value 0 or 1 if a level should be set, a value between 0 and 1 for the duty period of the PWM
     */
    this.setValue = (value) => {
        if (pinType < 3)
            throw new RangeError("pin " + index + " is set to input");
        if (value < 0 || value > 1)
            throw new RangeError("can only set pin to a value between 0 and 1");

        if (value > 0.0 && value < 1.0)  // PWM
            setPWM(index, value);
        else {
            disablePWM(index);

            if (index < 32) {
                if (value)
                    valuesLo |= 1 << index;
                else
                    valuesLo &= ~(1 << index);
            } else {
                if (value)
                    valuesHi |= 1 << index;
                else
                    valuesHi &= ~(1 << index);
            }
            native.gpioSetValues(valuesLo, valuesHi);
        }
    }

    /**
     * Callback for retrieval of the level or voltage (ADC) of an input pin
     * 
     * @callback GPIOGetValueCallback
     * @param {?Error} err optional error. If not null, the next parameters are not set
     * @param {Number} [value] if DIGITAL was used exactly 0 or 1, if ANALOG was used a value from 0 (= 0V) to 1 (= board voltage).
     */

    /**
     * Retrieves the level or voltage of the input pin
     *
     * @param {(DIGITAL|ANALOG)} [flags=DIGITAL] are exact levels requested or shall the voltage be retrieved via ADC?
     * @param {GPIOGetValueCallback} callback the callback called with the level or voltage
     */
    this.getValue = (flags, callback) => {
        if (typeof flags === 'function')
            handleGPIO(index, flags);
        else if (flags & exports.ANALOG)
            handleADC(index, callback);
        else
            handleGPIO(index, callback);
    }

    /**
     * Fired on GPIOPin, when the pin is set to INPUT, INPUT_PULLUP or INPUT_PULLDOWN
     * and the level of the pin rises to 1
     *
     * @event rise
     */

    /**
     * Fired on GPIOPin, when the pin is set to INPUT, INPUT_PULLUP or INPUT_PULLDOWN
     * and the level of the pin falls to 0
     *
     * @event fall
     */

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

isNeoniousOne = native.gpioSetCallback((index, rise) => {
    if (!internalPins[index])
        return;

    if (rise)
        internalPins[index].emit('rise');
    else
        internalPins[index].emit('fall');
});

/**
 * Access all pins via this array. The index is the pin number.
 *
 * @type {GPIOPin[]}
 */
exports.pins = [];

var i, last;
if (isNeoniousOne) {
    i = 1;
    last = 27;

    /**
     * The pin number for the red LED = 1. Only defined on the neonious one.
     * @type {Number}
     */
    exports.LED_RED = 1;

    /**
     * The pin number for the green LED = 2. Only defined on the neonious one.
     * @type {Number}
     */
    exports.LED_GREEN = 2;

    /**
     * The pin number for the user defined button = 27. Only defined on the neonious one.
     * @type {Number}
     */
    exports.BUTTON = 27;
} else {
    i = 0;
    last = 39;

    exports.ANALOG_ATTEN_DB_0 = 0;
    exports.ANALOG_ATTEN_DB_2_5 = 1;
    exports.ANALOG_ATTEN_DB_6 = 2;
    exports.ANALOG_ATTEN_DB_11 = 3;

    exports.setAnalogAttenuation = native.gpioSetAnalogAttenuation;
}
for (; i <= last; i++) {
    exports.pins[i] = internalPins[i] = new GPIOPin(i);
}

/**
 * Set the frequency of the PWM pins.
 *
 * @param {Number} frequency in Hz, default: 100 Hz
 */
exports.setFrequency = (frequency) => {
    let nanoSecs = 1000000000 / frequency;
    if (signalNanoSecs != nanoSecs) {
        signalNanoSecs = nanoSecs;
        sendPWMSignal();
    }
}

/**
 * Returns the frequency of the PWM pins.
 *
 * @returns {Number} in Hz, default: 100 Hz
 */
exports.getFrequency = () => {
    return 1000000000 / signalNanoSecs;
}

/**
 * Allows to set all pins which are set to output at once, via bit array. PWM pins are not modified.
 *
 * @param {Number} bits the values for the first 32 pins as bit array
 * @param {Number} [bitsHi] the values for the pins 33-39. Only required on plain ESP32-WROVER, as
 *                 the bit numbering on the device is clumbersome: Even though there are less
 *                 usable pins as with the neonious one, there are more pin numbers.
 */
exports.setValues = (bits, bitsHi) => {
    valuesLo = bits;
    if(bitsHi !== undefined)
        valuesHi = bitsHi;
    native.gpioSetValues(bits, bitsHi);
}

/**
 * Callback for retrieval of the levels of all input pins at once.
 * 
 * @callback GPIOGetValuesCallback
 * @param {?Error} err optional error. If not null, the next parameters are not set
 * @param {Number} [bits] the values for the first 32 pins as bit array
 * @param {Number} [bitsHi] the values for the pins 33-39. Only set on plain ESP32-WROVER, as
 *                 the bit numbering on the device is clumbersome: Even though there are less
 *                 usable pins as with the neonious one, there are more pin numbers.
 */

/**
 * Retrieves the levels of all pins which are set to input at once, via bit array.
 *
 * @param {GPIOGetValuesCallback} callback the callback called with the input levels
 */
exports.getValues = (callback) => {
    handleGPIO(null, callback);
}
