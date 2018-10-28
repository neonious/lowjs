'use script';

let native = require('native');

// TODO: make Hash a transform stream
class Hash {
    constructor(type) {
        this._native = native.createCryptoHash(this, type);
    }

    update(data, encoding) {
        if (typeof data === 'string')
            data = Buffer.from(data, encoding);

        native.cryptoHashUpdate(this._native, data);
        return this;
    }

    digest(encoding) {
        let val = new Buffer(20);
        native.cryptoHashDigest(this._native, val);
        if (encoding)
            return val.toString(encoding);
        else
            return val;
    }
}

// Fake Hmac to make express work
class Hmac {
    update() {
        return this;
    }

    digest() {
        return "cryptohmac";
    }

}

exports.createHash = function (type) {
    return new Hash(type);
}

exports.createHmac = function () {
    return new Hmac();
}