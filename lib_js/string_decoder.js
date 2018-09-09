'use strict';

class StringDecoder {
    constructor(encoding) {
        this._textDecoder = new TextDecoder(encoding);
    }

    write(buf) {
        return this._textDecoder.decode(buf, { 'stream': true });
    }

    end(buf) {
        return this._textDecoder.decode(buf);
    }
}

exports.StringDecoder = StringDecoder;
