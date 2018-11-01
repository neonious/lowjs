'use script';

let native = require('native');

// TODO: make Hash a transform stream
class Hash {
    constructor(type, key) {
        this._native = native.createCryptoHash(this, type, key);
    }

    update(data, encoding) {
        if (typeof data === 'string')
            data = Buffer.from(data, encoding);

        native.cryptoHashUpdate(this._native, data);
        return this;
    }

    digest(encoding) {
        let val = native.cryptoHashDigest(this._native);
        if (encoding)
            return val.toString(encoding);
        else
            return val;
    }
}

exports.createHash = function (type) {
    return new Hash(type);
}

exports.createHmac = function (type, key) {
    return new Hash(type, key);
}