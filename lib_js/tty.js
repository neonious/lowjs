'use strict';

let native = require('native');
let net = require('net');

let OSRelease;

const COLORS_2 = 1;
const COLORS_16 = 4;
const COLORS_256 = 8;
const COLORS_16m = 24;

// Some entries were taken from `dircolors`
// (https://linux.die.net/man/1/dircolors). The corresponding terminals might
// support more than 16 colors, but this was not tested for.
//
// Copyright (C) 1996-2016 Free Software Foundation, Inc. Copying and
// distribution of this file, with or without modification, are permitted
// provided the copyright notice and this notice are preserved.
const TERM_ENVS = [
    'Eterm',
    'cons25',
    'console',
    'cygwin',
    'dtterm',
    'gnome',
    'hurd',
    'jfbterm',
    'konsole',
    'kterm',
    'mlterm',
    'putty',
    'st',
    'terminator'
];

const TERM_ENVS_REG_EXP = [
    /ansi/,
    /color/,
    /linux/,
    /^con[0-9]*x[0-9]/,
    /^rxvt/,
    /^screen/,
    /^xterm/,
    /^vt100/
];

class ReadStream extends net.Socket {
    isTTY = true;
    isRaw = false;

    constructor(fd) {
        super({
            fd,
            readable: true,
            writable: false
        });
    }

    setRawMode(mode) {
        this.isRaw = !!mode;
        native.setsockopt(this._socketFD, undefined, undefined, undefined, this.isRaw);
    }
}

class WriteStream extends net.Socket {
    isTTY = true;

    constructor(fd) {
        super({
            fd,
            readable: false,
            writable: true
        });
    }

    // The `getColorDepth` API got inspired by multiple sources such as
    // https://github.com/chalk/supports-color,
    // https://github.com/isaacs/color-support.
    getColorDepth(env = process.env) {
        if (env.NODE_DISABLE_COLORS || env.TERM === 'dumb' && !env.COLORTERM) {
            return COLORS_2;
        }

        if (process.platform === 'win32') {
            // Lazy load for startup performance.
            if (OSRelease === undefined) {
                const { release } = require('os');
                OSRelease = release().split('.');
            }
            // Windows 10 build 10586 is the first Windows release that supports 256
            // colors. Windows 10 build 14931 is the first release that supports
            // 16m/TrueColor.
            if (+OSRelease[0] >= 10) {
                const build = +OSRelease[2];
                if (build >= 14931)
                    return COLORS_16m;
                if (build >= 10586)
                    return COLORS_256;
            }

            return COLORS_16;
        }

        if (env.TMUX) {
            return COLORS_256;
        }

        if (env.CI) {
            if ('TRAVIS' in env || 'CIRCLECI' in env || 'APPVEYOR' in env ||
                'GITLAB_CI' in env || env.CI_NAME === 'codeship') {
                return COLORS_256;
            }
            return COLORS_2;
        }

        if ('TEAMCITY_VERSION' in env) {
            return /^(9\.(0*[1-9]\d*)\.|\d{2,}\.)/.test(env.TEAMCITY_VERSION) ?
                COLORS_16 : COLORS_2;
        }

        switch (env.TERM_PROGRAM) {
            case 'iTerm.app':
                if (!env.TERM_PROGRAM_VERSION ||
                    /^[0-2]\./.test(env.TERM_PROGRAM_VERSION)) {
                    return COLORS_256;
                }
                return COLORS_16m;
            case 'HyperTerm':
            case 'Hyper':
            case 'MacTerm':
                return COLORS_16m;
            case 'Apple_Terminal':
                return COLORS_256;
        }

        if (env.TERM) {
            if (/^xterm-256/.test(env.TERM))
                return COLORS_256;

            const termEnv = env.TERM.toLowerCase();

            for (const term of TERM_ENVS) {
                if (termEnv === term) {
                    return COLORS_16;
                }
            }
            for (const term of TERM_ENVS_REG_EXP) {
                if (term.test(termEnv)) {
                    return COLORS_16;
                }
            }
        }

        if (env.COLORTERM)
            return COLORS_16;

        return COLORS_2;
    }
}

function isatty(fd) {
    return process.stdout.isTTY && (fd == 0 || fd == 1 || fd == 2);
}

// Backwards-compat
let readline;

WriteStream.prototype.cursorTo = function (x, y) {
    if (readline === undefined) readline = require('readline');
    readline.cursorTo(this, x, y);
};
WriteStream.prototype.moveCursor = function (dx, dy) {
    if (readline === undefined) readline = require('readline');
    readline.moveCursor(this, dx, dy);
};
WriteStream.prototype.clearLine = function (dir) {
    if (readline === undefined) readline = require('readline');
    readline.clearLine(this, dir);
};
WriteStream.prototype.clearScreenDown = function () {
    if (readline === undefined) readline = require('readline');
    readline.clearScreenDown(this);
};
WriteStream.prototype.getWindowSize = function () {
    return [this.columns, this.rows];
};

module.exports = { ReadStream, WriteStream, isatty };