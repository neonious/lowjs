Buffer = ((oldFunc) => {
    let newBuffer = function (...args) {
        if(typeof args[0] === 'string' && (args[1] == 'hex' || args[1] == 'base64'))
            return oldFunc.call(this, Duktape.dec(args[1], args[0]));
        else if(args[0].slice && typeof args[1] !== 'string' && args[2] !== undefined)
	    // Workaround: DukTape does not allow slicing in constructor
            return oldFunc.call(this, args[0].slice(args[1], args[1] + args[2]));
	else
            return oldFunc.call(this, ...args);
    }
    newBuffer.byteLength = oldFunc.byteLength;
    newBuffer.compare = oldFunc.compare;
    newBuffer.concat = oldFunc.concat;
    newBuffer.isBuffer = oldFunc.isBuffer;
    newBuffer.isEncoding = oldFunc.isEncoding;
    newBuffer.poolSize = oldFunc.poolSize;
    newBuffer.prototype = oldFunc.prototype;

    // Not implemented by DukTape
    newBuffer.from = (...args) => { return new newBuffer(...args); }
    newBuffer.allocUnsafe = newBuffer.alloc = (...args) => { return new newBuffer(...args); }
    return newBuffer;
})(Buffer);

Buffer.prototype.toString = ((oldFunc) => {
    return function (encoding, b, c) {
        if (encoding == 'hex' || encoding == 'base64')
            return Duktape.enc(encoding, this);
        else
            return oldFunc.call(this, encoding, b, c);
    }
})(Buffer.prototype.toString);

exports.Buffer = Buffer;
