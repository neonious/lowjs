let native = require('native');

class Script {
    constructor(code, options) {
        this._func = native.compile(code);
        if (options && options.filename)
            this._filename = options.filename;
    }

    runInNewContext(sandbox, options) {
        createContext(sandbox, options);
        return runInContext(sandbox, options);
    }

    runInContext(sandbox, options) {
        if (options && options.filename)
            this._func.fileName = options.filename;
        else
            this._func.fileName = this._filename;

        if (sandbox._vmIsRunning)
            sandbox._vmIsRunning++;
        else {
            sandbox._vmIsRunning = 1;
            for (let i = 0; i < native.jsProps.length; i++) {
                let name = native.jsProps[i];
                Object.defineProperty(sandbox, name,
                    Object.getOwnPropertyDescriptor(global, name));
            }
        }

        let res = native.runInContext(this._func, sandbox, options ? options.timeout : undefined, options ? !!options.breakOnSigint : undefined);
        if (!--sandbox._vmIsRunning) {
            delete sandbox._vmIsRunning;
            for (let i = 0; i < native.jsProps.length; i++) {
                try {   // we cannot remove unconfigurable elements... this is the best we can do
                    delete sandbox[native.jsProps[i]];
                } catch (e) { }
            }
        }

        return res;
    }

    runInThisContext(options) {
        return runInContext(global, options);
    }

    createCachedData() {
        // returning bogus data does not break anything
        // currently not supported, but we might in the future
        return new Buffer(1);
    }
}

function createScript(code, options) {
    return new Script(code, options);
}

function createContext(sandbox) {
    if (!sandbox)
        sandbox = {};
    sandbox._vmIsContext = true;
    return sandbox;
}

function isContext(sandbox) {
    return !!sandbox._vmIsContext;
}

function runInContext(code, sandbox, options) {
    let script = new Script(code, options);
    return script.runInContext(sandbox, options);
}

function runInNewContext(code, sandbox, options) {
    let script = new Script(code, options);
    createContext(sandbox, options);
    return script.runInContext(sandbox, options);
}

function runInThisContext(code, options) {
    let script = new Script(code, options);
    return script.runInThisContext(options);
}

module.exports = {
    Script,
    createScript,
    createContext,
    isContext,
    runInContext,
    runInNewContext,
    runInThisContext
};