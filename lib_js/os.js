'use strict';

export let EOL = '\n';

export function arch() {
    return process.arch;
}

export function platform() {
    return process.platform;
}

// TODO: fake below here
export function totalmem() {
    return 1024 * 1024 * 1024; // todo need this so that tests work
}

export function cpus() {
    return [{}]; // todo need this so that tests work
}

export function networkInterfaces() {
    return {}; // todo need this so that tests work
}