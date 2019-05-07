'use strict';

/**
 * Module for CAN interface. ESP32 has a CAN controller included, all you need is a CAN transciever.
 * @module can
 */

let events = require('events');
let native = require('native');

 /**
 * For CAN interface constructor. Fully functional mode (default)
 */
exports.MODE_NORMAL = 0;

/**
 * For CAN interface constructor. Does not send ACKs on reception. For testing
 */
exports.MODE_NO_ACK = 1;

/**
 * For CAN interface constructor. Does not send anything, receives in any case only
 */
exports.MODE_LISTEN_ONLY = 2;

/**
 * For CAN interface constructor. Default values for 25 kbit/s
 */
exports.TIMING_25_KBITS = {brp: 128, tseg_1: 16, tseg_2: 8, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 50 kbit/s
 */
exports.TIMING_50_KBITS = {brp: 80, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 100 kbit/s
 */
exports.TIMING_100_KBITS = {brp: 40, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 125 kbit/s
 */
exports.TIMING_125_KBITS = {brp: 32, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 250 kbit/s
 */
exports.TIMING_250_KBITS = {brp: 16, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 500 kbit/s
 */
exports.TIMING_500_KBITS = {brp: 8, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 800 kbit/s
 */
exports.TIMING_800_KBITS = {brp: 4, tseg_1: 16, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * For CAN interface constructor. Default values for 1 mbit/s
 */
exports.TIMING_1_MBITS = {brp: 4, tseg_1: 15, tseg_2: 4, sjw: 3, triple_sampling: false}

/**
 * Flag for transmit(). Do not resend if other node sends with priority
 */
exports.NO_RETRANSMISSION = 4;

/**
 * Flag for transmit(). Should be able to be received by itself
 */
exports.RECV_SELF = 8;

/**
 * Flag for transmit() and message event. Is a remote transmit request
 */
exports.REMOTE_TRANSMIT_REQUEST = 2;

/**
 * Now under 128 rxErrors or 128 txErrors
 */
exports.STATE_ERR_ACTIVE = 0;

/**
 * More than 128 rxError and 128 txError occurred
 */
exports.STATE_ERR_PASSIVE = 1;

/**
 * Too many errors occurred (rxError >= 256), interface no longer participates, intf.recover() must be called
 */
exports.STATE_BUS_OFF = 2;

/**
 * Recover is in progress (not finished until error count goes down)
 */
exports.STATE_RECOVERING = 3;

/**
 * intf.destroy() was called.
 */
exports.STATE_DESTROYED = 4;

/**
 * CAN interface 
 *
 * @extends events.EventEmitter
 * @fires message
 * @fires error
 * @fires stateChange
 * @fires rxMissed
 * @fires belowTXQueueThreshold
 * @fires aboveErrorThreshold
 * @fires belowErrorThreshold
 * @property {Number} state The state of the interface according to CAN2.0B Specification. Can be one of
 *      can.STATE_ERR_ACTIVE: Now under 128 rxErrors or 128 txErrors
 *      can.STATE_ERR_PASSIVE: More than 128 rxError and 128 txError occurred
 *      can.STATE_BUS_OFF: Too many errors occurred (rxError >= 256), interface no longer participates in the event.
 *           intf.recover() must be called
 *      can.STATE_RECOVERING: Recover is in progress (not finished until error count goes down)
 *      can.STATE_DESTROYED: intf.destroy() was called.
 * @property {Number} txQueueCount Number of messages that are to be sent (=intf.transmit 
 *          calls that still have not caused a callback call
 * @property {Number} txQueueThreshold If intf.txQueueCount falls below this value again, the event belowTXQueueThreshold is called. Default: 0
 * @property {Number} rxQueueCount Number of messages that have been received but for which the message event could not be called yet.
 * @property {Number} errorThreshold If txError and rxError are above this value, the event aboveErrorThreshold is called,
				as soon as this is no longer the case, belowErrorThreshold is called. Default: 96
 * @property {Object} stats
 * @property {Number} stats.rxMissed Number of unsaved CAN messages due to too small RX buffer
 * @property {Number} stats.arbLost Number of sent packets where another node has priority
 * @property {Number} stats.busError Number of bus errors
 * @property {Number} stats.rxError Number of receive errors after CAN2.0B. Also goes down, not only up
 * @property {Number} stats.txError Number of transmit errors according to CAN2.0B. Also goes down, not only up
 ***/
class CAN extends events.EventEmitter {
    /**
     * Creates a CAN interface.
     *
     * Destroy explicitly with destroy() when the interface is no longer in use.
     *
     * @param {Object} options The options
     * @param {Number} options.pinRX the RX pin. Use pin 24-26 on neonious one
     * @param {Number} options.pinTX the TX pin. Use pin 24-25 on neonious one
     * @param {Number} [options.pinBusOff] optional output pin: High normally, low if interface is in BUS_OFF state
     * @param {Number} [options.pinClkOut] optional output pin: The clock signal
     * @param {Number} [options.mode=can.MODE_NORMAL] can.MODE_NORMAL: fully functional (default)
     *                      can.MODE_NO_ACK: does not send ACKs on reception. For testing
	 *                      can.MODE_LISTEN_ONLY: does not send anything, receives in any case only
     * @param {Number} [options.rxQueueSize=128] number of messages to queue till sent to user program.
     * @param {Number} [options.txQueueSize=128] number of messages to allow in send queue
     * @param {Object} [options.timing=can.TIMING_500_KBITS] timing parameters. Either one of can.TIMING_*, or an object with the properties brp, tseg_1, tseg_2, sjw, triple_sampling. For more information, see https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/can.html
     * @param {Object} [options.filter] Allows the device to receive only data for certain CAN codes at the hardware level.
		Useful to avoid overloading the main program in JavaScript
		either {id: Number, id_len: Number} lets the interface receive only message from one ID, id_len should be 11 or 29 ---
		or {code: Number, mask: Number, single: Boolean}
		gives the interface the full flexibility of the CAN driver, with the option to filter multiple IDs.
		For an explanation of the subvalues of this full specification, see
https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/c^  an.html

		To reference: To map from the simple to the complete specification:
		code := id << (32 - id_len)
		mask := ~((((1 << id_len) - 1) << (32 - id_len)))
		single := true
     */
    constructor(options) {
        // Might throw
        if(!options.timing)
            options.timing = exports.TIMING_500_KBITS;
        native.canConstruct(options, (event, a, b, c, d) => {
            if(event == 'message')
                a = new Buffer(a);  // so readUInt* is defined

            this.emit(event, a, b, c, d);
        });

        Object.defineProperty(this, 'state', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return exports.STATE_DESTROYED;
                return native.canProperties().state;
            }
        });
        Object.defineProperty(this, 'txQueueCount', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return undefined;
                return native.canProperties().txQueueCount;
            }
        });
        Object.defineProperty(this, 'txQueueThreshold', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return undefined;
                return native.canProperties().txQueueThreshold;
            },
            set: function (val) {
                if(this._isDestroyed)
                    return;
                native.canSetProperty(0, val);
            }
        });
        Object.defineProperty(this, 'rxQueueCount', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return undefined;
                return native.canProperties().rxQueueCount;
            }
        });
        Object.defineProperty(this, 'errorThreshold', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return undefined;
                return native.canProperties().errorThreshold;
            },
            set: function (val) {
                if(this._isDestroyed)
                    return;
                native.canSetProperty(1, val);
            }
        });
        Object.defineProperty(this, 'stats', {
            enumerable: true,
            get: function () {
                if(this._isDestroyed)
                    return {};
                return native.canProperties().stats;
            }
        });
    }

    /**
     * Fires on received message
     *
     * @event message
     * @param {Buffer} buffer The data, maximum 8 bytes
     * @param {Number} id The id of the recipient
     * @param {Number} id_len 11 or 29
     * @param {Number} [flags=0] Bitfield of
            can.REMOTE_TRANSMIT_REQUEST is a remote transmit request
     */

    /**
     * Might be called on generic error not to do with actions of user.
     * If no callback was specified for an intf.transmit or intf.recover, errors of these calls are also reported here.
     *
     * @event error
     * @param {Error} err
					If err.code == 'CAN_TX_QUEUE_FULL', then too many transmit() calls are executed at the same time
(see constructor option txQueueSize).
					If err.code == 'CAN_ARB_LOST', then another node has sent with priority.
						(should only possible if can.NO_RETRANSMISSION is set)
					If err.code == 'CAN_RECOVER_INITIATED', then a recover process was started.
                    If err.code == 'CAN_DESTROYED', destroy() was already called.
                        Callback is not called on existing transmits() on call of destroy()
					Otherwise there was a generic error
     */

    /**
     * Fires on state change according to CAN2.0B Specification.
     *
     * @event stateChange
     * @param {Number} state
     *      can.STATE_ERR_ACTIVE: Now under 128 rxErrors or 128 txErrors
	 *      can.STATE_ERR_PASSIVE: More than 128 rxError and 128 txError occurred
	 *      can.STATE_BUS_OFF: Too many errors occurred (rxError >= 256), interface no longer participates in the event.
     *           intf.recover() must be called
     *      can.STATE_RECOVERING: Recover is in progress (not finished until error count goes down). Data to be sent is buffered
     *                            in the TX queue.
     *      can.STATE_DESTROYED: intf.destroy() was called.
     */

    /**
     * Too many messages in the RX queue, messages had to be thrown away
		before the event message could be called for them.
		Remedy: Increase rxQueueSize in the constructor, set filter to ID in the constructor or use less blocking code.
     * Event might be called only once for multiple errors of this type.
     *
     * @event rxMissed
     */

    /**
     * TX queue is now smaller than intf.txQueueThreshold. New data can therefore be sent.
     *
     * @event belowTXQueueThreshold
     */

    /**
     * txError and rxError are both above or equal to intf.errorThreshold (default 96). Can be used to monitor CAN bus quality.
     *
     * @event aboveErrorThreshold
     */

    /**
     * One of txError and rxError is again under intf.errorThreshold (default 96). Can be used to monitor CAN bus quality.
     *
     * @event belowErrorThreshold
     */

     /**
     * Frees all resources of the interface, allowing the program to use the pins differently or
     * construct a new interface with other parameters.
     */
    destroy() {
        if(this._isDestroyed)
            return;
        this._isDestroyed = true;
        native.canDestruct();
    }


    /**
     * Switches from can.STATE_BUS_OFF to can.STATE_RECOVERING and then to can.STATE_ERR_ACTIVE as soon as error count has fallen.
	 * If another recover process is running, the processes merges and both callbacks are called after the recover.
	 * If no recover is necessary, the callback is called immediately, with no error.
     *
     * @param {Boolean} [clearTXQueue=false] If true, the TX queue is emptied if a recover is necessary.
     * @param {CANCallback} [callback] If specified, it will be called as soon as the recover process is finished
     * @returns {CAN} interface itself, to chain call other methods
     */
    recover(clearTXQueue, callback) {
        if(this._isDestroyed) {
            let err = new Error('CAN interface already destroyed.');
            err.code = 'CAN_DESTROYED';
            if(callback)
                callback(err);
            else
                this.emit('error', err);
            return;
        }

        if(this._recoverQueue) {
            this._recoverQueue.push(callback);
        } else {
            if(callback)
                this._recoverQueue = [callback];
            else
                this._recoverQueue = [];
            native.canRecover((err) => {
                let callbacks = this._recoverQueue;
                delete this._recoverQueue;

                if(callbacks.length) {
                    for(let i = 0; i < callbacks.length; i++)
                        callbacks[i](err);
                } else if(err)
                    this.emit('error', err);
            });
        }
        return this;
    }

    /**
     * Sends a message
     *
     * @param {Buffer} buffer The data, maximum 8 bytes
     * @param {Number} id The id of the recipient
     * @param {Number} id_len 11 or 29
     * @param {Number} [flags=0] Bitfield of
			can.NO_RETRANSMISSION do not resend if other node sends with priority
			can.RECV_SELF should be able to be received by itself
            can.REMOTE_TRANSMIT_REQUEST is a remote transmit request
     * @param {CANCallback} [callback]
            If specified, it will be called as soon as the message is shipped.
     * @returns {CAN} interface itself, to chain call other methods
     */
    transmit(buffer, id, id_len, flags, callback) {
        if(this._isDestroyed) {
            let err = new Error('CAN interface already destroyed.');
            err.code = 'CAN_DESTROYED';
            if(callback)
                callback(err);
            else
                this.emit('error', err);
            return;
        }

        if(flags === undefined)
            flags = 0;
        else if(typeof flags !== 'number') {
            callback = flags;
            flags = 0;
        }

        native.canSend(buffer, id, id_len, flags, (err) => {
            if(callback)
                callback(err);
            else if(err)
                this.emit('error', err);
        });
        return this;
    }

    /**
     * Tells the interface to keep the program running when a transfer is taking place.
     * This is the default.
     *
     * @returns {CAN} interface itself, to chain call other methods
     */
    ref() {
        if(this._isDestroyed)
            return;
        native.canRef(true);
        return this;
    }

    /**
     * Tells the interface to not keep the program running when a transfer is taking place,
     * but there is nothing else to do.
     *
     * @returns {CAN} interface itself, to chain call other methods
     */
    unref() {
        if(this._isDestroyed)
            return;
        native.canRef(false);
        return this;
    }

    /**
     * Callback which is called when a message is sent (transmit()) or a recover is done (recover())
     *
     * @callback CANCallback
     * @param {?Error} err null if no error, otherwise
					If err.code == 'CAN_TX_QUEUE_FULL', then too many transmit() calls are executed at the same time. 
                        (see constructor option txQueueSize)
					If err.code == 'CAN_ARB_LOST', then another node has sent with priority.
						(should only possible if can.NO_RETRANSMISSION is set)
					If err.code == 'CAN_RECOVER_INITIATED', then a recover process was started.
                    If err.code == 'CAN_DESTROYED', destroy() was already called.
                        Callback is not called on existing transmits() on call of destroy()
					Otherwise there was a generic error
     */
}
exports.CAN = CAN;
