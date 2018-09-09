'use strict';

let native = require('native');

exports.open = native.open;
exports.close = native.close;

exports.read = (fd, buffer, offset, length, position, callback) => {
    let cb;
    function res(err, bytesRead) {
        cb(err, bytesRead, buffer);
    }

    if (callback === undefined) {
        if (position === undefined) {
            if (length === undefined) {
                cb = offset;
                native.read(fd, buffer, 0, buffer.length, null, res);
            } else {
                cb = length;
                native.read(fd, buffer, offset, buffer.length - offset, null, res);
            }
        } else {
            cb = position;
            native.read(fd, buffer, offset, length, null, res);
        }
    } else {
        cb = callback;
        native.read(fd, buffer, offset, length, position, res);
    }
};

exports.write = (fd, buffer, offset, length, position, callback) => {
    let cb;
    function res(err, bytesWritten) {
        cb(err, bytesWritten, buffer);
    }

    if (callback === undefined) {
        if (position === undefined) {
            if (length === undefined) {
                cb = offset;
                native.write(fd, buffer, 0, buffer.length, null, res);
            } else {
                cb = length;
                native.write(fd, buffer, offset, buffer.length - offset, null, res);
            }
        } else {
            cb = position;
            native.write(fd, buffer, offset, length, null, res);
        }
    } else {
        cb = callback;
        native.write(fd, buffer, offset, length, position, res);
    }
};

exports.fstat = native.fstat;
exports.openSync = native.openSync;

exports.closeSync = (fd) => {
    let resErr;
    native.close(fd, (err) => {
        resErr = err;
    });
    native.waitDone(fd);
    if (resErr)
        throw new Error(resErr);
};

exports.readSync = (fd, buffer, offset, length, position) => {
    let resErr, resBytesRead;
    function res(err, bytesRead) {
        resErr = err;
        resBytesRead = bytesRead;
    }

    if (position === undefined) {
        if (length === undefined) {
            if (offset == undefined)
                native.read(fd, buffer, 0, buffer.length, null, res);
            else
                native.read(fd, buffer, offset, buffer.length - offset, null, res);
        } else
            native.read(fd, buffer, offset, length, null, res);
    } else
        native.read(fd, buffer, offset, length, position, res);
    native.waitDone(fd);

    if (resErr)
        throw new Error(resErr);
    return resBytesRead;
};

exports.writeSync = (fd, buffer, offset, length, position) => {
    let resErr, resBytesWritten;
    function res(err, bytesWritten) {
        resErr = err;
        resBytesWritten = bytesWritten;
    }

    if (position === undefined) {
        if (length === undefined) {
            if (offset == undefined)
                native.write(fd, buffer, 0, buffer.length, null, res);
            else
                native.write(fd, buffer, offset, buffer.length - offset, null, res);
        } else
            native.write(fd, buffer, offset, length, null, res);
    } else
        native.write(fd, buffer, offset, length, position, res);
    native.waitDone(fd);

    if (resErr)
        throw new Error(resErr);
    return resBytesWritten;
};

exports.fstatSync = (fd) => {
    let resErr, resStat;
    native.fstat(fd, (err, stat) => {
        resErr = err;
        resStat = stat;
    });
    native.waitDone(fd);
    if (resErr)
        throw new Error(resErr);
    return resStat;
};

exports.readFile = (path, options, callback) => {
    if (!callback) {
        callback = options;
        options = null;
    } else if (typeof options === 'string')
        options = { 'encoding': options };

    exports.open(path, options && options.flags ? options.flags : 'r', (err, fd) => {
        if (err) {
            callback(err);
            return;
        }

        let stat = exports.fstat(fd, (err, stat) => {
            if (err) {
                exports.close(fd, () => {
                    callback(err);
                });
                return;
            }

            let buf = new Buffer(stat.size);
            exports.read(fd, buf, 0, stat.size, null, (err) => {
                if (err) {
                    exports.close(fd, () => {
                        callback(err);
                    });
                    return;
                }

                exports.close(fd, (err) => {
                    if (err) {
                        callback(err);
                        return;
                    }

                    if (options && options.encoding)
                        callback(null, buf.toString(options.encoding));
                    else
                        callback(null, buf);
                });
            });
        });
    });
};

exports.readFileSync = (path, options) => {
    if (typeof options === 'string')
        options = { 'encoding': options };

    let fd = exports.openSync(path, options && options.flags ? options.flags : 'r');
    try {
        let stat = exports.fstatSync(fd);
        let buf = new Buffer(stat.size);
        exports.readSync(fd, buf, 0, stat.size, null);
    } catch (e) {
        exports.close(fd);
        throw e;
    }
    exports.close(fd);

    if (options && options.encoding)
        return buf.toString(options.encoding);
    else
        return buf;
};

exports.writeFile = (path, data, options, callback) => {
    if (!callback) {
        callback = options;
        options = null;
    } else if (typeof options === 'string')
        options = { 'encoding': options };
    if (typeof data === 'string')
        data = new Buffer(data, options && options.encoding ? options.encoding : 'utf8');

    exports.open(path, options && options.flags ? options.flags : 'w', (err, fd) => {
        if (err) {
            callback(err);
            return;
        }

        exports.write(fd, data, 0, data.length, null, (err) => {
            if (err) {
                exports.close(fd, () => {
                    callback(err);
                });
                return;
            }

            exports.close(fd, (err) => {
                if (err) {
                    callback(err);
                    return;
                }

                callback(null);
            });
        });
    });
};

exports.writeFileSync = (path, data, options) => {
    if (typeof options === 'string')
        options = { 'encoding': options };
    if (typeof data === 'string')
        data = new Buffer(data, options && options.encoding ? options.encoding : 'utf8');

    let fd = exports.openSync(path, options && options.flags ? options.flags : 'w');
    try {
        exports.writeSync(fd, data, 0, data.length, null);
    } catch (e) {
    }
    exports.close(fd);
};