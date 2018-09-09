'use script';
throw new Error('crypto module is not implemented yet')

class Hash {
    update() {
        return this;
    }

    digest() {
        return "cryptohash";
    }
}

class Hmac {
    update() {
        return this;
    }

    digest() {
        return "cryptohmac";
    }

}

exports.createHash = function () {
    return new Hash();
}

exports.createHmac = function () {
    return new Hmac();
}