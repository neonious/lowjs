// Note: we are skipping init.js and main.js, because both are modules which cannot be require()-ed
exports.builtinModules = [
    'assert',
    'buffer',
    'console',
    'crypto',
    'dgram',
    'dns',
    'events',
    'fs',
    'http',
    'https',
    'module',
    'net',
    'path',
    'process',
    'querystring',
    'readline',
    'repl',
    'stream',
    'string_decoder',
    'tls',
    'tty',
    'url',
    'util',
    'vm',
    'zlib'
]
if (process.platform == 'esp32') {
    exports.builtinModules.push('gpio');
    exports.builtinModules.push('i2c');
    exports.builtinModules.push('lowsys');
    exports.builtinModules.push('sdcard');
    exports.builtinModules.push('signal');
    exports.builtinModules.push('spi');
    exports.builtinModules.push('uart');
}