let native = require('native');

class I2C {
    constructor(options, optionsOld) {
        // options:
        // clockHz
        // pinMISO
        // pinMOSI

        if (optionsOld)             // backwards compat, will be removed
            options = optionsOld;   // end of 2019

        this._index = native.initPeripherial(2, options);
        this._holdRef = true;
        this._ref = false;
        this._pipe = [];
    }

    destroy() {
        native.destroyPeripherial(this._index);
    }

    transfer(address, data, bytesRead, callback) {
        if (this._ref) {
            this._pipe.push([address, data, callback]);
            return;
        }
        if (!data) {
            callback(null);
            return;
        }

        if (this._holdRef)
            native.runRef(1);
        this._ref = true;

        let dataOut = bytesRead ? new Buffer(bytesRead) : null;
        native.transferPeripherial(this._index, address, data, dataOut, (err) => {
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
    flush(callback) {
        if (this._ref)
            this._pipe.push([null, null, callback]);
        else
            callback(null);
    }

    ref() {
        if (this._ref && !this._holdRef)
            native.runRef(1);
        this._holdRef = true;
        return this;
    }

    unref() {
        if (this._ref && this._holdRef)
            native.runRef(-1);
        this._holdRef = false;
        return this;
    }
}

module.exports = {
    I2C
};