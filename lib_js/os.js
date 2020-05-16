'use strict';

const native = require('native');

export let EOL = '\n';

export function arch() {
    return process.arch;
}

export function platform() {
    return process.platform;
}

export function freemem() {
    return native.osInfo().freemem;
}

export function totalmem() {
    return native.osInfo().totalmem;
}

export function uptime() {
    return native.osInfo().uptime;
}

// TODO: fake below here
exports.constants = {
};

export function homedir() {
    return "/";
}

export function tmpdir() {
    switch(process.platform) {
        case "linux":
            return "/tmp";
        default:
            return "/";
    }
}

export function hostname() {
    return "host";
}

export function loadavg() {
    return [0, 0, 0];
}

export function endianness() {
    return 'LE';
}

export function networkInterfaces() {
    return {};
}

export function release() {
    return process.platform;
}

export function type() {
    return process.platform;
}

export function cpus() {
    return [{}]; // todo need this so that tests work
}

export function networkInterfaces() {
    return {}; // todo need this so that tests work
}

export function userInfo() {
    return {};
}